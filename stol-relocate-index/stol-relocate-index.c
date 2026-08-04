// Copyright (C) 2026 Applied Intuition
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// stol-relocate-index: a tool to trick a git index into accepting a new working directory
//
// When you copy a git repo, as in `cp -a repo repo_copy`, the index at
// repo_copy/.git/index is invalidated. It contains metadata, namely `ctime`
// and `ino` that is not correct for the newly-copied files.
//
// Git will notice this invalidation and repair it automatically, but the
// process is slow. This tool makes the operation fast, by poking the correct
// values for `ctime` and `ino` into the existing binary file.

#include <fcntl.h>
#include <liburing.h>
#include <linux/stat.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define CACHE_SIGNATURE 0x44495243

#define CE_EXTENDED (0x4000)
#define CE_NAMEMASK (0x0fff)
#define CE_INTENT_TO_ADD (1 << 29)
#define CE_SKIP_WORKTREE (1 << 30)
#define CE_EXTENDED_FLAGS (CE_INTENT_TO_ADD | CE_SKIP_WORKTREE)

#define BATCH_SIZE (1024)

#define ENTRY_CTIME_SEC_OFFSET (0)
#define ENTRY_CTIME_NSEC_OFFSET (4)
#define ENTRY_INO_OFFSET (20)
#define ENTRY_UID_OFFSET (28)
#define ENTRY_GID_OFFSET (32)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

#define assert(c) while(!(c)) __builtin_unreachable()
#define append(b, v) do { assert((b).len < (b).cap); (b).data[(b).len++] = v; } while(0)
struct u8pbuf {
  u8 **data;
  ptrdiff_t len, cap;
};

u16 read_u16(u8 **data) {
  u16 v = ((*data)[1] << 0) | ((*data)[0] << 8);
  *data += 2;
  return v;
}

u32 read_u32(u8 **data) {
  u32 v = ((*data)[3] << 0) | ((*data)[2] << 8) | ((*data)[1] << 16) | ((*data)[0] << 24);
  *data += 4;
  return v;
}

void write_u32(u8 *data, u32 v) {
  data[0] = (v >> 24) & 0xff;
  data[1] = (v >> 16) & 0xff;
  data[2] = (v >>  8) & 0xff;
  data[3] = (v >>  0) & 0xff;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: index <index path>\n");
    return 1;
  }

  char *index_path = argv[1];
  int fd = open(index_path, O_RDWR);
  if (fd == -1) {
    fprintf(stderr, "Failed to open index file %s\n", index_path);
    return 1;
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    fprintf(stderr, "Failed to stat index file %s\n", index_path);
    return 1;
  }

  if (sb.st_size < 12 + 20) { // signature, version, entry count, checksum trailer
    fprintf(stderr, "Index file %s is too small to be a git index\n", index_path);
    return 1;
  }

  u8 *data = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap index file %s\n", index_path);
    return 1;
  }
  u8 *end = data + sb.st_size;

  u32 sig = read_u32(&data);
  if (sig != CACHE_SIGNATURE) {
    fprintf(stderr, "Unexpected cache signature: %x\n", sig);
    return 1;
  }

  u32 version = read_u32(&data);

  u32 expand_name_field = version == 4;
  if (expand_name_field) {
    fprintf(stderr, "Do not yet support expand_name_field\n");
    return 1;
  }

  u32 entries = read_u32(&data);

  struct u8pbuf entry_starts = {
    .data = malloc(BATCH_SIZE * sizeof(u8 *)),
    .len = 0,
    .cap = BATCH_SIZE,
  };

  struct io_uring ring;
  i32 initret = io_uring_queue_init(BATCH_SIZE, &ring, 0);
  if (initret < 0) {
    fprintf(stderr, "Queue init error %s\n", strerror(-initret));
    return 1;
  }
  struct statx *stx = calloc(BATCH_SIZE, sizeof(*stx));
  const char **names = malloc(BATCH_SIZE * sizeof(*names));

  for (u32 batch = 0, entries_seen = 0; batch < (entries / BATCH_SIZE) + 1; ++batch) {
    entry_starts.len = 0; // clear

    for (u32 i = 0; (i < BATCH_SIZE) && (entries_seen++ < entries); ++i) {
      u8 *start_entry = data;
      if (end - data < 62) { // stat data + sha + flags
        fprintf(stderr, "Corrupt index: entry extends past end of file\n");
        return 1;
      }
      append(entry_starts, start_entry);

      data += 20; // ctime_s, ns, mtime, dev
      data += 40; // ino, mode, uid, gid, size, sha

      u32 flags = read_u16(&data);
      u32 namelen = flags & CE_NAMEMASK;

      if (flags & CE_EXTENDED) {
        if (end - data < 2) {
          fprintf(stderr, "Corrupt index: entry extends past end of file\n");
          return 1;
        }
        u16 flag2 = read_u16(&data);
        u32 extended_flags = flag2 << 16;

        if (extended_flags & ~CE_EXTENDED_FLAGS) {
          fprintf(stderr, "Invalid extended flags %x\n", extended_flags);
          return 1;
        }

        flags |= extended_flags;
      }

      if (namelen >= 4095) {
        fprintf(stderr, "can't handle long name yet\n");
        return 1;
      }

      // the name and at least one NUL padding byte must fit in the mapping.
      if (end - data < (ptrdiff_t) namelen + 1) {
        fprintf(stderr, "Corrupt index: entry name extends past end of file\n");
        return 1;
      }

      struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
      io_uring_prep_statx(sqe, AT_FDCWD, (const char *)data, 0, STATX_INO | STATX_CTIME | STATX_UID | STATX_GID, &stx[i]);
      names[i] = (const char *)data;
      io_uring_sqe_set_data64(sqe, i);

      data += namelen;

      u32 entrylen = data - start_entry;
      u32 padlen = (8 - (entrylen % 8)) ;
      if (!padlen) padlen = 8;

      data += padlen;
      if (data > end) {
        fprintf(stderr, "Corrupt index: entry padding extends past end of file\n");
        return 1;
      }
    }

    i32 waitres = io_uring_submit_and_wait(&ring, entry_starts.len);
    if (waitres < 0 || (u32) waitres != entry_starts.len) {
      fprintf(stderr, "Unexpected result from submit_and_wait: %d\n", waitres);
      return 1;
    }

    struct io_uring_cqe *cqe;
    u32 head;
    i32 done = 0;

    io_uring_for_each_cqe(&ring, head, cqe) {
      u32 i = (u32) io_uring_cqe_get_data64(cqe);
      assert((ptrdiff_t) i < entry_starts.len);
      struct statx *res = &stx[i];
      ++done;

      if (cqe->res < 0) {
        fprintf(stderr, "%s Error %s\n", names[i], strerror(-cqe->res));
        continue;
      }

      write_u32(entry_starts.data[i] + ENTRY_CTIME_SEC_OFFSET,  (u32) res->stx_ctime.tv_sec);
      write_u32(entry_starts.data[i] + ENTRY_CTIME_NSEC_OFFSET, (u32) res->stx_ctime.tv_nsec);
      write_u32(entry_starts.data[i] + ENTRY_INO_OFFSET,        (u32) res->stx_ino);
      write_u32(entry_starts.data[i] + ENTRY_UID_OFFSET,        (u32) res->stx_uid);
      write_u32(entry_starts.data[i] + ENTRY_GID_OFFSET,        (u32) res->stx_gid);
    }

    io_uring_cq_advance(&ring, done);
  }

  io_uring_queue_exit(&ring);

  // The index file ends with a checksum, which is invalid after our edits. We
  // zero it explicitly, which matches git's index.skipHash behavior.
  memset(end - 20, 0, 20);

  return 0;
}
