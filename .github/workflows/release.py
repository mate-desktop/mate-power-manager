#!/usr/bin/python3
# -*- encoding:utf-8 -*-
# FileName: release.py
# SPDX-License-Identifier: GPL-2.0-or-later

__author__ = "yetist"
__copyright__ = "Copyright (c) 2025 yetist <yetist@gmail.com>"
__license__ = "MIT"

import os
import sys
import json
import requests
import hmac
import hashlib
import uuid
import datetime
import subprocess
import shlex

URL = "https://release.mate-desktop.org/release"


def send_post_to_url(payload):
    headers = {}
    nonce = str(uuid.uuid4()).replace("-", "").upper()
    if type(payload) is str:
        body = payload
    else:
        body = json.dumps(payload, indent=2, default=str)

    secret = os.getenv("API_SECRET", "")
    if not secret:
        print('Please set the "API_SECRET" environment variable for secure transfer.')
    else:
        data = nonce + body
        signature = hmac.new(
            bytes(secret, "utf8"), msg=bytes(data, "utf8"), digestmod=hashlib.sha256
        )
        sign = signature.hexdigest().upper()
        headers["X-Build-Signature"] = sign

    headers["X-Build-Nonce"] = nonce
    headers["User-Agent"] = "docker-build/0.1.2 (Travis CI)"
    try:
        r = requests.post(URL, data=body, headers=headers)
        if r.status_code != 200:
            print(f"Visit {URL} : {r.status_code} {r.reason}")
            return False
        else:
            print(r.text)
            print("%s has been notified" % URL)
            return True
    except requests.exceptions.ConnectionError as error:
        print(f"Connect Error {URL} : {error}")
        return False


def get_git_log(repo_name: str, old_version: str, new_version: str):
    lines = subprocess.getoutput(
        f'git log --pretty="- %s" {old_version}..{new_version}'
    ).splitlines()
    lines.insert(0, f"### {repo_name} {new_version}")
    lines.insert(1, "")
    return lines


def get_news_log(repo_name: str, old_version: str, new_version: str):
    lines = []
    if not os.path.isfile("NEWS"):
        print('"NEWS" file lost.')
        return lines
    old = f"{repo_name} {old_version}"
    new = f"{repo_name} {new_version}"

    found = False
    data = open("NEWS").read().splitlines()
    for i in data:
        line = i.strip()
        if line.startswith("##") and line.endswith(new):
            found = True
    if not found:
        print(f'forgot to update the "NEWS" file for {repo_name}-{new}?')
        return lines

    found = False
    for i in data:
        line = i.strip()
        if line.startswith("##") and line.endswith(new):
            found = True
        if line.startswith("##") and line.endswith(old):
            found = False
        if found:
            lines.append(line)
    return lines


def sha256sum(path):
    fobj = open(path, "rb")
    fobj.seek(0)
    sha256sum = hashlib.sha256(fobj.read()).hexdigest()
    fobj.close()

    base_name = os.path.basename(path)
    sum_path = os.path.join("{}.sha256sum".format(base_name))
    sobj = open(sum_path, "w+")
    sobj.write(f"{sha256sum}  {base_name}\n")
    sobj.close()


def notify_server(repo: str, version: str, body: str, flist: list[str]):
    data = {}
    data["name"] = repo.split("/")[1]
    data["version"] = version
    data["tag"] = f"v{version}"
    data["repo"] = repo
    data["draft"] = False
    data["news"] = body
    data["prerelease"] = False
    data["created_at"] = datetime.datetime.now(datetime.UTC).isoformat()
    data["published_at"] = datetime.datetime.now(datetime.UTC).isoformat()
    data["files"] = []
    for i in flist:
        file = {}
        basename = os.path.basename(i)
        file["name"] = basename
        file["size"] = os.path.getsize(i)
        file["url"] = (
            f"https://github.com/{repo}/releases/download/v{version}/{basename}"
        )
        data["files"].append(file)
    payload = json.dumps(data, indent=2, default=str)
    if not send_post_to_url(payload):
        print("We can not send post to the release server")
        sys.exit(1)


def main():
    old_tag = subprocess.getoutput(
        "gh release ls -L 1 --json tagName --jq '.[0].tagName'"
    ).strip()
    old_version = old_tag[1:]
    new_tag = os.getenv("GITHUB_REF_NAME", "")
    if len(new_tag.strip()) == 0:
        print("no tag")
        sys.exit(1)
    if old_tag == new_tag:
        print(f"{new_tag} already releaed")
        sys.exit(1)
    new_version = new_tag[1:]

    repo = os.getenv("GITHUB_REPOSITORY", "")
    repo_name = repo.split("/")[1]

    logs = get_news_log(repo_name, old_version, new_version)
    if not logs:
        logs = get_git_log(repo_name, old_version, new_version)

    logs.insert(
        0,
        f"Changes since the last release: https://github.com/{repo}/compare/{old_tag}...{new_tag}",
    )
    logs.insert(1, "")

    body = "\n".join(logs)

    with open(".release.note.txt", "w") as f:
        f.write(body)

    title = f"{repo_name} {new_version} release"

    # release version
    if os.path.isfile(f"_build/meson-dist/{repo_name}-{new_version}.tar.xz"):
        # meson dist
        cmdline = f'gh release create {new_tag} --title "{title}" -F .release.note.txt _build/meson-dist/*'
        x = subprocess.run(shlex.split(cmdline))
        if x.returncode != 0:
            sys.exit(1)

        flist = [
            f"_build/meson-dist/{repo_name}-{new_version}.tar.xz",
            f"_build/meson-dist/{repo_name}-{new_version}.tar.xz.sha256sum",
        ]
        notify_server(repo, new_version, body, flist)
    else:
        # autotools make distcheck
        tarfile = f"{repo_name}-{new_version}.tar.xz"
        if not os.path.exists(f"{tarfile}.sha256sum"):
            sha256sum(tarfile)

        cmdline = f'gh release create {new_tag} --title "{title}" -F .release.note.txt {repo_name}-*.tar.xz*'
        x = subprocess.run(shlex.split(cmdline))
        if x.returncode != 0:
            sys.exit(1)

        flist = [
            f"{repo_name}-{new_version}.tar.xz",
            f"{repo_name}-{new_version}.tar.xz.sha256sum",
        ]
        notify_server(repo, new_version, body, flist)


if __name__ == "__main__":
    main()
