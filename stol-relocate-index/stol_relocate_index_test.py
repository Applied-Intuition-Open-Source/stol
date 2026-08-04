# Copyright (C) 2026 Applied Intuition
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest
import pathlib
import subprocess
import tempfile
import shutil

EXEC_PATH = pathlib.Path(__file__).parent / "stol-relocate-index"

class ProjRelocateIndexTest(unittest.TestCase):
  def test_relocate(self):
    FILES_IN_REPO = 5000

    with tempfile.TemporaryDirectory() as canon_name, tempfile.TemporaryDirectory() as copy_name:
      canon = pathlib.Path(canon_name)
      copy = pathlib.Path(copy_name)

      for i in range(FILES_IN_REPO):
        with open(canon / f'file_{i}.txt', 'w') as of:
          of.write(f'this is file {i}')

      subprocess.run(["git", "init", "--quiet"], cwd=canon, check=True)
      subprocess.run(["git", "add", "."], cwd=canon, check=True)
      # Disable background maintenance
      subprocess.run(["git", "-c", "maintenance.auto=false", "-c", "gc.auto=0",
                      "commit", "--quiet", "-m", "check in files"], cwd=canon, check=True)
      subprocess.run(["git", "diff-files", "--quiet"], cwd=canon, check=True)

      shutil.copytree(canon, copy, dirs_exist_ok=True)
      diffret = subprocess.run(["git", "diff-files", "--quiet"], cwd=copy)
      self.assertEqual(diffret.returncode, 1) # git notices something is wrong

      subprocess.run([EXEC_PATH, ".git/index"], cwd=copy, check=True)

      diffret = subprocess.run(["git", "diff-files", "--quiet"], cwd=copy)
      self.assertEqual(diffret.returncode, 0) # git can no longer tell something is wrong

  def test_empty_index(self):
    with tempfile.TemporaryDirectory() as d:
      index = pathlib.Path(d) / 'index'
      index.touch()
      result = subprocess.run([EXEC_PATH, index], capture_output=True)
      self.assertEqual(result.returncode, 1) # clean error, not a crash

  def test_truncated_index(self):
    # build a small real index, then cut it off mid-entry
    with tempfile.TemporaryDirectory() as repo_name:
      repo = pathlib.Path(repo_name)
      for i in range(10):
        with open(repo / f'file_{i}.txt', 'w') as of:
          of.write(f'this is file {i}')

      subprocess.run(["git", "init", "--quiet"], cwd=repo, check=True)
      subprocess.run(["git", "add", "."], cwd=repo, check=True)

      index = repo / '.git' / 'index'
      data = index.read_bytes()
      with open(index, 'wb') as of:
        of.write(data[:len(data) // 2])

      result = subprocess.run([EXEC_PATH, ".git/index"], cwd=repo, capture_output=True)
      self.assertEqual(result.returncode, 1) # clean error, not a crash


if __name__ == "__main__":
  unittest.main()
