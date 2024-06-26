#!/usr/bin/env bash

scriptdir="$( dirname -- "$BASH_SOURCE"; )";
docker run --rm --name doc_server -p 8080:80 -v "${scriptdir}/output/build/CodeCoverage/Coverage/":/usr/share/nginx/html:ro -d nginx
