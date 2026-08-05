# stol

`stol` is a suite of tools that make it easy to create and manage git
worktrees. It uses reflinks to make worktree creation fast and cheap even on
huge monorepos.

In order to use `stol`, you need a Linux filesystem that supports reflinks. The root filesystem in most Linux installations does not support reflinks, so most users will have to create a new fileystem in order to use `stol`.

There are two ways to provide one.

* As a disk image: create a large file on your existing disk, format it as XFS,
  and mount it. `stol create-image` sets this up.
* As a dedicated disk: format a spare physical disk as XFS and mount it. `stol
  format-drive` sets this up.

`stol` contains helpers for both options.

Although `stol` has only been tested on XFS, it may be possible to use other filesystems like btrfs.

## Install

### Dependencies

Building `stol` requires `liburing`, a C compiler, and make

```sh
sudo apt install liburing-dev gcc make
```

At runtime stol uses:
- `git`: required
- `xfsprogs`: required for most users. supports `stol create-image` and `stol format-drive`
- `sgdisk`: required only for `stol format-drive`
- `gh`:  optional, supports worktree creation by github PR URL

```
sudo apt install git xfsprogs gdisk
```

`gh` can be installed following [these instructions](https://cli.github.com/).

### Installing

Install stol to a location on your `PATH`:

```sh
make install
```

This builds and installs all executables to `~/.local/bin`. You may override the install location with `PREFIX`.

```sh
make install PREFIX=/usr/local
```

Be sure to follow the post-install instructions to finish setting up your environment.

## Setup

With stol installed, provision the reflink-capable filesystem it will use for
`$STOL_ROOT`. By convention we mount it at `/mnt/work`, but any location works as
long as your user owns the mount point.

First create the mount point:

```sh
sudo mkdir /mnt/work
sudo chown $USER:$USER /mnt/work
```

Then provision the filesystem with one of the helpers (see the two options
above) and mount it there:

- `stol create-image`: create and format a disk image
- `stol format-drive`: format a dedicated physical disk

`stol format-drive` must run as root. Be careful to use the correct disk in
this command, or you may lose data.

```sh
sudo "$(command -v stol-format-drive)" /dev/diskname
```

## Directory layout

`stol` prescribes a particular directory layout under `$STOL_ROOT`.

```
$STOL_ROOT/projects/<repository name>/<worktree name>
```

Each time you `stol import` a new repository, you get a new directory under `$STOL_ROOT` for that repo. That new directory contains each of its worktrees as subdirectories.

Your projects directory will look something like this:

```
/mnt/work/projects $ tree -L 2 -a
.
├── my-project.git
│   ├── .repo
│   ├── .stol
│   ├── 00-main
│   ├── task-1
│   ├── task-2
│   └── task-3
└── stol.git
    ├── .repo
    ├── .stol
    ├── 00-main
    └── feature-1
```

There are four kinds of directories under a project:

1. `.repo`: the main checkout of the repo. You should never have to think about
   this.
2. `00-{branchname}`: a "template" worktree. Most of the time you will have
   exactly one, with the main branch checked out.
3. `.stol`: a place for your repo-specific hooks.
4. The remaining directories are worktrees created by `stol new`.

When you import a project with `stol import`, it creates a `.repo` directory and
an initial "template" worktree.

When you create a new worktree with `stol new`, it makes a reflinked copy
of the template worktree. If you have multiple template worktrees,
`stol new` prompts you to select one.

In order to pick up the latest changes in your template worktree, you can run
`stol sync <project>`. It is also safe to run `git pull` yourself.

You can conveniently delete worktrees with `stol rm`. It takes several forms:

```sh
stol rm .                    # delete the current worktree
stol rm <worktree>           # delete a worktree by name in this project
stol rm <project>/<worktree> # delete a worktree by name in any project
```

## scd

`scd` is an interactive shell function that makes it quick and easy to jump
into a worktree, even if the target worktree does not yet exist.

Installation:

```sh
# bash users
source /path/to/stol.git/src/stol.bash

# zsh users
source /path/to/stol.git/src/stol.zsh

# fish users
source /path/to/stol.git/src/stol.fish
```

Usage:

```sh
scd                          # cd to the projects root
scd <project>                # cd to a project directory
scd <project>/<worktree>     # cd to a specific worktree
```

`scd` can create new worktrees implicitly:

```sh
scd -n <name> [<project>]    # create a new worktree (new branch)
scd -e <branch> [<project>]  # check out an existing branch in a new worktree
scd <pr-url>                 # check out a GitHub PR branch in a new worktree
```

With `-n` and `-e`, the project can be omitted if your working directory is
already in a project.

If the worktree already exists, `-e` and PR URLs will cd there without
recreating it.

The PR URL form is shorthand for `-e`. It resolves the branch name and project
from the URL via `gh`, so instead of:

```sh
scd -e someone/branch-name repo-name
```

you can do:

```sh
scd https://github.com/Applied-Intuition-Open-Source/stol/pull/1
```

## Hooks

`stol` runs two kinds of hooks, both executables at `.stol/hooks/post-new` and
`.stol/hooks/pre-remove`, looked up in two places:

- **Repo-tracked hooks**, committed to the repo itself, so they ship with the
  code and go through normal review:

  ```
  my-project/                 (a git repository)
  ├── .stol/
  │   └── hooks/
  │       ├── post-new
  │       └── pre-remove
  └── ...
  ```

  Because worktrees are reflinked copies of a template branch, whatever the
  repo commits at this path is already present in every new worktree — no
  installation step required.

- **Project-level hooks**, host-specific and not tracked by git, for personal
  or machine-specific customization:

  ```
  my-project.git/              (the stol project directory, under $STOL_ROOT)
  ├── .stol/
  │   └── hooks/
  │       ├── post-new
  │       └── pre-remove
  ├── .repo/
  └── 00-main/
  ```

If both exist, the repo-tracked hook runs first, then the project-level hook —
so personal hooks can assume the repo's own baseline automation already ran.

- **post-new**: runs after the worktree is created, inside the new worktree
  directory.
- **pre-remove**: Runs inside the worktree directory, before it is deleted.

## Extending stol

You can add new stol subcommands by placing an executable prefixed with `stol-`
in your `PATH`. `stol my-subcmd` will call `stol-my-subcmd` internally.
