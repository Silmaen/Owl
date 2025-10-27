#!/usr/bin/env bash

script_dir="$( dirname -- "$BASH_SOURCE"; )";
docker run --rm --name doc_server -p 8080:80 -v "${script_dir}/output/build/CodeCoverage_clang/Coverage/":/usr/share/nginx/html:ro -d nginx
