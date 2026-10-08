"""Package list pushed to the Conan binary cache (`conan/upload_list.py`)."""

import importlib.util

from ci import root

_spec = importlib.util.spec_from_file_location("owl_upload_list", root / "conan" / "upload_list.py")
assert _spec is not None and _spec.loader is not None
upload_list = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(upload_list)


def test_only_local_binaries_of_the_graph_are_pushed() -> None:
    graph = {
        "graph": {
            "nodes": {
                "0": {"ref": "conanfile", "package_id": None, "binary": None},
                "1": {"ref": "zlib/1.3.2#aaa%1.0", "package_id": "p1", "binary": "Build"},
                "2": {"ref": "glad/2.0.8#bbb", "package_id": "p2", "binary": "Download"},
                "3": {"ref": "spdlog/1.17.0#ccc", "package_id": "p3", "binary": "Cache"},
            }
        }
    }
    revision = {"timestamp": 1.0}
    listing = {
        "Local Cache": {
            "zlib/1.3.2": {
                "revisions": {
                    "aaa": {
                        **revision,
                        "packages": {"p1": {"revisions": {"r1": {}}}, "other": {"revisions": {"r2": {}}}},
                    }
                }
            },
            "glad/2.0.8": {"revisions": {"bbb": {**revision, "packages": {"p2": {"revisions": {"r3": {}}}}}}},
            "spdlog/1.17.0": {"revisions": {"old": {**revision, "packages": {"p3": {"revisions": {"r4": {}}}}}}},
            "nfd/1.2.1": {"revisions": {"ddd": {**revision, "packages": {"p4": {"revisions": {"r5": {}}}}}}},
        }
    }

    kept = upload_list.filter_listing(listing, upload_list.wanted_binaries(graph))["Local Cache"]

    assert list(kept) == ["zlib/1.3.2"]
    assert list(kept["zlib/1.3.2"]["revisions"]["aaa"]["packages"]) == ["p1"]
