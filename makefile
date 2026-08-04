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

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/stol

RELOCATE_INDEX = stol-relocate-index/stol-relocate-index

.PHONY: test install uninstall $(RELOCATE_INDEX)

test:
	bash scripts/test

$(RELOCATE_INDEX):
	$(MAKE) -C stol-relocate-index release

install: $(RELOCATE_INDEX)
	mkdir -p $(BINDIR)
	install -m 755 src/stol src/stol-* $(BINDIR)
	install -m 755 $(RELOCATE_INDEX) $(BINDIR)
	mkdir -p $(DATADIR)
	install -m 644 src/stol.bash src/stol.zsh src/stol.fish $(DATADIR)
	@echo
	@echo "Installed stol to $(BINDIR)"
	@printf '\033[1mTo complete installation, add the following to your shell config:\033[0m\n'
	@echo
	@echo 'export STOL_ROOT=/mnt/work # a location owned by your user'
	@echo 'export STOL_PREFIX=$(USER)  # optional branch prefix '
	@echo 'export PATH="$$PATH:$(BINDIR)"'
	@echo 'source $(DATADIR)/stol.bash # or .zsh / .fish'

uninstall:
	rm -f $(BINDIR)/stol $(addprefix $(BINDIR)/,$(notdir $(wildcard src/stol-*)))
	rm -f $(BINDIR)/stol-relocate-index
	rm -rf $(DATADIR)
