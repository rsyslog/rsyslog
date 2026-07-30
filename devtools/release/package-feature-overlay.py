#!/usr/bin/env python3
"""Apply the daily-stable feature contract to native distro packaging."""

import argparse
import json
import pathlib
import re
import sys


def fail(message):
    raise SystemExit(message)


def replace_once(text, pattern, replacement, label):
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        fail(f"could not find unique {label}")
    return re.sub(pattern, replacement, text, count=1)


def load_contract(path):
    contract = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    yaml_feature = contract.get("yaml", {})
    if yaml_feature.get("required_in_base") is not True:
        fail("feature contract must require YAML support in the base package")
    modules = contract.get("module_packages", [])
    if len(modules) != 1 or modules[0].get("module") != "omazuredce":
        fail("prototype contract must define exactly the omazuredce module package")
    return contract, yaml_feature, modules[0]


def require_dependency(declarations, dependency, source_name):
    dependency_pattern = rf"(?<![A-Za-z0-9_.+-]){re.escape(dependency)}(?![A-Za-z0-9_.+-])"
    if not re.search(dependency_pattern, "\n".join(declarations)):
        fail(f"{source_name} does not declare required YAML dependency {dependency}")


def debian_build_dependencies(control):
    source_stanza = re.split(r"\n\s*\n", control, maxsplit=1)[0]
    lines = source_stanza.splitlines()
    declarations = []
    for index, line in enumerate(lines):
        match = re.match(r"^Build-Depends:\s*(.*)$", line)
        if not match:
            continue
        declarations.append(match.group(1))
        for continuation in lines[index + 1:]:
            if not continuation.startswith((" ", "\t")):
                break
            declarations.append(continuation.strip())
    return declarations


def rpm_build_dependencies(spec):
    return re.findall(r"(?m)^BuildRequires:\s*(.*)$", spec)


def alpine_build_dependencies(apkbuild):
    match = re.search(
        r"(?ms)^makedepends=(?P<quote>['\"])(?P<value>.*?)(?P=quote)\s*$",
        apkbuild,
    )
    if not match:
        fail("could not find Alpine makedepends assignment")
    return [match.group("value")]


def apply_debian(packaging_dir, contract_path):
    root = pathlib.Path(packaging_dir)
    contract, yaml_feature, module = load_contract(contract_path)
    del contract
    control_path = root / "control"
    rules_path = root / "rules"
    if not control_path.is_file() or not rules_path.is_file():
        fail(f"missing Debian control or rules file below {root}")

    control = control_path.read_text(encoding="utf-8")
    rules = rules_path.read_text(encoding="utf-8")
    require_dependency(
        debian_build_dependencies(control),
        yaml_feature["build_dependencies"]["deb"],
        "Debian Build-Depends",
    )
    if "--disable-libyaml" in rules:
        fail("Debian rules explicitly disable required YAML support")

    package_name = module["package_names"]["deb"]
    if not re.search(rf"(?m)^Package: {re.escape(package_name)}$", control):
        stanza = (
            f"\nPackage: {package_name}\n"
            "Architecture: any\n"
            "Depends: ${shlibs:Depends},\n"
            "         ${misc:Depends},\n"
            "         rsyslog (= ${binary:Version})\n"
            f"Description: {module['summary']}\n"
            " This package provides the omazuredce output module for sending logs to\n"
            " Microsoft Azure Monitor through a Data Collection Endpoint.\n"
        )
        control = control.rstrip() + "\n" + stanza

    configure_flag = module["configure_flag"]
    if configure_flag not in rules:
        rules = replace_once(
            rules,
            r"(?m)^(\s*)(--enable-omprog\s+\\)$",
            rf"\g<1>{configure_flag} \\" + "\n" + r"\g<1>\g<2>",
            "Debian configure option anchor",
        )

    manifest = root / f"{package_name}.install"
    expected_path = f"usr/lib/${{DEB_HOST_MULTIARCH}}/rsyslog/{module['module_file']}"
    if manifest.exists():
        entries = manifest.read_text(encoding="utf-8").splitlines()
        if expected_path not in entries:
            fail(f"existing {manifest.name} does not own {expected_path}")
    else:
        manifest.write_text(expected_path + "\n", encoding="utf-8")

    control_path.write_text(control, encoding="utf-8")
    rules_path.write_text(rules, encoding="utf-8")


def rpm_package_block(module, flavor):
    package_name = module["package_names"][flavor]
    suffix = package_name.removeprefix("rsyslog-")
    if flavor == "opensuse":
        return (
            f"%package {suffix}\n"
            "Requires:       %{name} = %{version}\n"
            f"Summary:        {module['summary']}\n"
            "Group:          System/Daemons\n\n"
            f"%description {suffix}\n"
            "Rsyslog is an enhanced multi-threaded syslog daemon. See rsyslog\n"
            "package.\n\n"
            "This module sends logs to Microsoft Azure Monitor through a Data\n"
            "Collection Endpoint.\n\n"
        )
    return (
        f"%package {suffix}\n"
        f"Summary: {module['summary']}\n"
        "Requires: %name = %version-%release\n\n"
        f"%description {suffix}\n"
        "This module sends logs to Microsoft Azure Monitor through a Data\n"
        "Collection Endpoint.\n\n"
    )


def apply_rpm(spec_path, contract_path, flavor):
    path = pathlib.Path(spec_path)
    contract, yaml_feature, module = load_contract(contract_path)
    del contract
    text = path.read_text(encoding="utf-8")
    require_dependency(
        rpm_build_dependencies(text),
        yaml_feature["build_dependencies"]["rpm"],
        "RPM BuildRequires",
    )
    if "--disable-libyaml" in text:
        fail("RPM spec explicitly disables required YAML support")

    package_name = module["package_names"][flavor]
    suffix = package_name.removeprefix("rsyslog-")
    if not re.search(rf"(?m)^%package\s+{re.escape(suffix)}$", text):
        text = replace_once(text, r"(?m)^%prep\s*$", rpm_package_block(module, flavor) + "%prep", "RPM %prep")

    configure_flag = module["configure_flag"]
    if configure_flag not in text:
        text = replace_once(
            text,
            r"(?m)^(\s*--enable-omhttp\s*\\)$",
            rf"\t{configure_flag} \\" + "\n" + r"\g<1>",
            "RPM configure option anchor",
        )

    if flavor == "opensuse" and module["module_file"] not in text:
        text = replace_once(
            text,
            r"(?m)^(\s*omhttpfs\.so\s*\\)$",
            rf"\1\n\t\t{module['module_file']} \\",
            "openSUSE module relocation anchor",
        )

    files_header = f"%files {suffix}"
    module_macro = "%{rsyslog_module_dir_withdeps}" if flavor == "opensuse" else "%{_libdir}/rsyslog"
    if not re.search(rf"(?m)^{re.escape(files_header)}$", text):
        files = f"{files_header}\n{module_macro}/{module['module_file']}\n\n"
        text = replace_once(text, r"(?m)^%changelog\s*$", files + "%changelog", "RPM %changelog")

    path.write_text(text, encoding="utf-8")


def apply_alpine(apkbuild_path, contract_path):
    path = pathlib.Path(apkbuild_path)
    contract, yaml_feature, module = load_contract(contract_path)
    del contract
    text = path.read_text(encoding="utf-8")
    require_dependency(
        alpine_build_dependencies(text),
        yaml_feature["build_dependencies"]["apk"],
        "Alpine makedepends",
    )
    if "--disable-libyaml" in text:
        fail("APKBUILD explicitly disables required YAML support")

    package_name = module["package_names"]["apk"]
    plugin_name = package_name.removeprefix("rsyslog-")
    plugins_match = re.search(r'(?ms)^_plugins="\n(?P<body>.*?)\n\s*"$', text)
    if not plugins_match:
        fail("could not find Alpine _plugins block")
    plugin_entries = [line.strip() for line in plugins_match.group("body").splitlines()]
    if plugin_name not in plugin_entries:
        text = replace_once(
            text,
            r"(?m)^(\s*omotel\s*)$",
            rf"\1\n\t{plugin_name}",
            "Alpine plugin package anchor",
        )

    configure_flag = module["configure_flag"]
    if configure_flag not in text:
        text = replace_once(
            text,
            r"(?m)^(\s*--enable-omotel\s*\\)$",
            rf"\1\n\t\t{configure_flag} \\",
            "Alpine configure option anchor",
        )
    path.write_text(text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    debian = subparsers.add_parser("debian")
    debian.add_argument("packaging_dir")
    debian.add_argument("contract")
    rpm = subparsers.add_parser("rpm")
    rpm.add_argument("spec")
    rpm.add_argument("contract")
    rpm.add_argument("flavor", choices=("rpm", "opensuse"))
    alpine = subparsers.add_parser("alpine")
    alpine.add_argument("apkbuild")
    alpine.add_argument("contract")
    args = parser.parse_args()

    if args.command == "debian":
        apply_debian(args.packaging_dir, args.contract)
    elif args.command == "rpm":
        apply_rpm(args.spec, args.contract, args.flavor)
    elif args.command == "alpine":
        apply_alpine(args.apkbuild, args.contract)


if __name__ == "__main__":
    try:
        main()
    except (KeyError, json.JSONDecodeError) as error:
        print(f"invalid package feature contract: {error}", file=sys.stderr)
        raise SystemExit(1) from error
