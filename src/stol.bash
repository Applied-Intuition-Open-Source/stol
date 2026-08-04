#!/bin/bash

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


# Source this file from your shell config:
#   source /path/to/scd.bash

scd() {
  local dest
  dest="$(stol scd "$@")" || return
  cd "$dest"
}

_scd() {
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local projects_dir="$STOL_ROOT/projects"

  [[ -d "$projects_dir" ]] || return

  if [[ "$cur" == */* ]]
  then
    local project="${cur%%/*}"
    local project_dir="$projects_dir/$project"
    [[ -d "$project_dir" ]] || return

    local prefix="${cur#*/}"
    local entries
    entries=$(cd "$project_dir" && compgen -d -- "$prefix" | grep -v '^\.')
    COMPREPLY=( $(printf "$project/%s\n" $entries) )
  else
    COMPREPLY=( $(cd "$projects_dir" && compgen -d -- "$cur" | grep -v '^\.') )
  fi
}

complete -o nospace -S / -F _scd scd
