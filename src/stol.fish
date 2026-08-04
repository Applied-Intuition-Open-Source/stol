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

# Source this file from your Fish config:
#   source /path/to/stol.fish

function scd
    set -l dest (stol scd $argv)
    or return
    cd $dest
end

function __scd_complete
    set -l projects_dir "$STOL_ROOT/projects"
    test -d $projects_dir; or return

    set -l token (commandline -ct)

    if string match -q "*/*" -- $token
        set -l project (string replace -r "/.*" "" -- $token)
        set -l project_dir "$projects_dir/$project"
        test -d $project_dir; or return

        set -l prefix (string replace -r "^[^/]*/" "" -- $token)
        for entry in $project_dir/$prefix*/
            set -l name (basename $entry)
            set -l rel (string replace "$project_dir/" "" -- $entry)
            string match -qv ".*" -- $name; and echo "$project/$rel"
        end
    else
        for entry in $projects_dir/$token*/
            set -l name (basename $entry)
            string match -qv ".*" -- $name; and echo "$name/"
        end
    end
end

complete -c scd -f -a '(__scd_complete)'
