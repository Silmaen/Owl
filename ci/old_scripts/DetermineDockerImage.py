#!/usr/bin env python3
"""
Just determine the docker image based on the json file.
"""
from argparse import ArgumentParser
from json import load
from pathlib import Path

parser = ArgumentParser("simple input.")
parser.add_argument("preset", type=str, help="The preset to check.")
args = parser.parse_args()

curPath = Path(__file__).resolve().parent
with open(curPath / "dockerImages.json") as js:
    data = load(js)

image = "notfound"
if args.preset in data["images"]:
    result = data["images"][args.preset]
    if "image" in result:
        image = result["image"]
    if "build_platform" in result:
        print(f"##teamcity[setParameter name='docker_build_platform' value='{result['build_platform']}']")
    if "test_platform" in result:
        print(f"##teamcity[setParameter name='docker_test_platform' value='{result['test_platform']}']")
print(f"##teamcity[setParameter name='docker_image' value='{image}']")

if image == "notfound":
    exit(1)
exit(0)
