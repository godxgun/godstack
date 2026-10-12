# Godstack website

This Hugo source tree is separate from the Godstack code repository.

```sh
export GODSTACK_SITE="${GODSTACK_SITE:-$HOME/rsr/src/site}"
# Use the location of your Godstack code checkout:
export GODSTACK_ROOT="${GODSTACK_ROOT:-$HOME/rsr/src/sigma-c/godstack}"
python3 "$GODSTACK_ROOT/tools/generate_docs.py"
python3 "$GODSTACK_ROOT/tools/generate_docs.py" --check
hugo --source "$GODSTACK_SITE" --destination /tmp/godstack-site --noBuildLock
python3 "$GODSTACK_ROOT/tools/test_site.py" /tmp/godstack-site
```

The generator reads headers from its own code checkout and writes only generated
API blocks/search data here, preserving handwritten articles. Browser/demo tools
also honor GODSTACK_SITE; individual tools accept output-directory overrides.

Rend2, its draft docs, and its demos/checks live in `~/rsr/src/godstack-pro`, not
in this website or the Godstack code checkout. No hosted repository URL for
this site or godstack-pro has been established; CI needs an explicitly supplied
site checkout to validate publication artifacts. Generated `public/` output
must be rebuilt rather than hand-edited.

The licensing files in `static/` are publication copies from Godstack. Keep them
synchronized when licensing changes; preserve independent asset licenses.
Do not relabel old browser binaries; keep their original MIT and Emscripten
notices, and rebuild from exact source before publishing new binaries.
