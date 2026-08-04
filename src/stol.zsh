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
#   source /path/to/stol.zsh

scd() {
  local dest
  dest="$(stol scd "$@")" || return
  cd "$dest"
}

_scd() {
  local projects_dir="$STOL_ROOT/projects"
  [[ -d "$projects_dir" ]] || return

  local cur="${words[CURRENT]}"

  if [[ "$cur" == */* ]]; then
    local project="${cur%%/*}"
    local project_dir="$projects_dir/$project"
    [[ -d "$project_dir" ]] || return

    local prefix="${cur#*/}"
    local entries=( "${project_dir}"/${prefix}*(N-/) )
    compadd -S / -p "${project}/" -- "${entries[@]##*/}"
  else
    local entries=( "${projects_dir}"/${cur}*(N-/) )
    compadd -S / -- "${entries[@]##*/}"
  fi
}

compdef _scd scd
