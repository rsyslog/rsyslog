#!/usr/bin/env python3
"""Apply the daily-stable feature and package-profile contract."""

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
    module_names = {module.get("module") for module in modules}
    required_modules = {"openssl", "gnutls", "omotel"}
    if not required_modules.issubset(module_names):
        fail("feature contract must define openssl, gnutls, and omotel module packages")
    if len(module_names) != len(modules):
        fail("feature contract contains duplicate module names")

    profiles = contract.get("profiles", [])
    profile_names = {profile.get("profile") for profile in profiles}
    if profile_names != {"standard", "full"}:
        fail("feature contract must define standard and full profiles")
    if len(profile_names) != len(profiles):
        fail("feature contract contains duplicate profile names")
    for profile in profiles:
        unknown = set(profile.get("modules", [])) - module_names
        if unknown:
            fail(f"profile {profile['profile']} contains unknown modules: {sorted(unknown)}")
    return yaml_feature, modules, profiles


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


def add_configure_flag(text, configure_flag, pattern, label, indentation=None):
    if configure_flag in text:
        return text
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        fail(f"could not find unique {label}")
    match = matches[0]
    if indentation is None:
        indentation = match.group(1)
    addition = f"{indentation}{configure_flag} \\\n"
    return text[:match.start()] + addition + text[match.start():]


def debian_module_stanza(module):
    package_name = module["package_names"]["deb"]
    description = module["description"]
    return (
        f"\nPackage: {package_name}\n"
        "Architecture: any\n"
        "Depends: ${shlibs:Depends},\n"
        "         ${misc:Depends},\n"
        "         rsyslog (= ${binary:Version})\n"
        f"Description: {module['summary']}\n"
        f" {description}\n"
    )


def debian_profile_stanza(profile, modules_by_name):
    dependencies = ["${misc:Depends}", "rsyslog (= ${binary:Version})"]
    dependencies.extend(
        f"{modules_by_name[name]['package_names']['deb']} (= ${{binary:Version}})"
        for name in profile["modules"]
    )
    formatted_dependencies = ",\n         ".join(dependencies)
    return (
        f"\nPackage: {profile['package_names']['deb']}\n"
        "Architecture: all\n"
        f"Depends: {formatted_dependencies}\n"
        f"Description: {profile['summary']}\n"
        " This empty package installs a version-locked, cross-distribution rsyslog\n"
        " functionality profile.\n"
    )


def normalize_debian_install_path(path):
    """Normalize Debian multiarch spellings used in baseline install manifests."""
    return path.replace("${DEB_HOST_MULTIARCH}", "*")


def apply_debian(packaging_dir, contract_path):
    root = pathlib.Path(packaging_dir)
    yaml_feature, modules, profiles = load_contract(contract_path)
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

    for module in modules:
        package_name = module["package_names"]["deb"]
        if not re.search(rf"(?m)^Package: {re.escape(package_name)}$", control):
            control = control.rstrip() + "\n" + debian_module_stanza(module)

        rules = add_configure_flag(
            rules,
            module["configure_flag"],
            r"(?m)^(\s*)--enable-omprog\s+\\$",
            "Debian configure option anchor",
        )

        manifest = root / f"{package_name}.install"
        expected_path = f"usr/lib/${{DEB_HOST_MULTIARCH}}/rsyslog/{module['module_file']}"
        if manifest.exists():
            entries = manifest.read_text(encoding="utf-8").splitlines()
            normalized_expected_path = normalize_debian_install_path(expected_path)
            if normalized_expected_path not in {
                normalize_debian_install_path(entry) for entry in entries
            }:
                fail(f"existing {manifest.name} does not own {expected_path}")
        else:
            manifest.write_text(expected_path + "\n", encoding="utf-8")

    modules_by_name = {module["module"]: module for module in modules}
    for profile in profiles:
        package_name = profile["package_names"]["deb"]
        if not re.search(rf"(?m)^Package: {re.escape(package_name)}$", control):
            control = control.rstrip() + "\n" + debian_profile_stanza(profile, modules_by_name)

    control_path.write_text(control, encoding="utf-8")
    rules_path.write_text(rules, encoding="utf-8")


def rpm_exact_requirement(package_name, flavor):
    if flavor == "opensuse":
        return f"{package_name} = %{{version}}-%{{release}}"
    return f"{package_name} = %version-%release"


def rpm_module_block(module, flavor):
    package_name = module["package_names"][flavor]
    suffix = package_name.removeprefix("rsyslog-")
    group = "Group:          System/Daemons\n" if flavor == "opensuse" else ""
    base_name = "%{name}" if flavor == "opensuse" else "%name"
    return (
        f"%package {suffix}\n"
        f"Requires:       {rpm_exact_requirement(base_name, flavor)}\n"
        f"Summary:        {module['summary']}\n"
        f"{group}\n"
        f"%description {suffix}\n"
        f"{module['description']}\n\n"
    )


def rpm_profile_block(profile, modules_by_name, flavor):
    package_name = profile["package_names"][flavor]
    suffix = package_name.removeprefix("rsyslog-")
    requirements = [rpm_exact_requirement("%{name}" if flavor == "opensuse" else "%name", flavor)]
    requirements.extend(
        rpm_exact_requirement(modules_by_name[name]["package_names"][flavor], flavor)
        for name in profile["modules"]
    )
    requirement_lines = "".join(f"Requires:       {requirement}\n" for requirement in requirements)
    group = "Group:          System/Daemons\n" if flavor == "opensuse" else ""
    return (
        f"%package {suffix}\n"
        "BuildArch:      noarch\n"
        f"{requirement_lines}"
        f"Summary:        {profile['summary']}\n"
        f"{group}\n"
        f"%description {suffix}\n"
        "This empty package installs a version-locked, cross-distribution rsyslog\n"
        "functionality profile.\n\n"
    )


def apply_rpm(spec_path, contract_path, flavor):
    path = pathlib.Path(spec_path)
    yaml_feature, modules, profiles = load_contract(contract_path)
    text = path.read_text(encoding="utf-8")
    require_dependency(
        rpm_build_dependencies(text),
        yaml_feature["build_dependencies"]["rpm"],
        "RPM BuildRequires",
    )
    if "--disable-libyaml" in text:
        fail("RPM spec explicitly disables required YAML support")

    for module in modules:
        package_name = module["package_names"][flavor]
        suffix = package_name.removeprefix("rsyslog-")
        if not re.search(rf"(?m)^%package\s+{re.escape(suffix)}$", text):
            text = replace_once(text, r"(?m)^%prep\s*$", rpm_module_block(module, flavor) + "%prep", "RPM %prep")

        text = add_configure_flag(
            text,
            module["configure_flag"],
            r"(?m)^(\s*)--enable-omhttp\s*\\$",
            "RPM configure option anchor",
            "\t",
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

    modules_by_name = {module["module"]: module for module in modules}
    for profile in profiles:
        package_name = profile["package_names"][flavor]
        suffix = package_name.removeprefix("rsyslog-")
        if not re.search(rf"(?m)^%package\s+{re.escape(suffix)}$", text):
            text = replace_once(
                text,
                r"(?m)^%prep\s*$",
                rpm_profile_block(profile, modules_by_name, flavor) + "%prep",
                "RPM %prep",
            )
        files_header = f"%files {suffix}"
        if not re.search(rf"(?m)^{re.escape(files_header)}$", text):
            text = replace_once(text, r"(?m)^%changelog\s*$", files_header + "\n\n%changelog", "RPM %changelog")

    path.write_text(text, encoding="utf-8")


def alpine_profile_function(profile, modules_by_name):
    dependencies = [f"$pkgname=$pkgver-r$pkgrel"]
    dependencies.extend(
        f"{modules_by_name[name]['package_names']['apk']}=$pkgver-r$pkgrel"
        for name in profile["modules"]
    )
    depends = " ".join(dependencies)
    return (
        f"\n_profile_{profile['profile']}() {{\n"
        f"\tpkgdesc=\"{profile['summary']}\"\n"
        f"\tdepends=\"{depends}\"\n"
        "\tmkdir -p \"$subpkgdir\"\n"
        "}\n"
    )


def apply_alpine(apkbuild_path, contract_path):
    path = pathlib.Path(apkbuild_path)
    yaml_feature, modules, profiles = load_contract(contract_path)
    text = path.read_text(encoding="utf-8")
    require_dependency(
        alpine_build_dependencies(text),
        yaml_feature["build_dependencies"]["apk"],
        "Alpine makedepends",
    )
    if "--disable-libyaml" in text:
        fail("APKBUILD explicitly disables required YAML support")

    plugins_match = re.search(r'(?ms)^_plugins="\n(?P<body>.*?)\n\s*"$', text)
    if not plugins_match:
        fail("could not find Alpine _plugins block")
    plugin_entries = [line.strip().split(":", 1)[0] for line in plugins_match.group("body").splitlines()]
    for module in modules:
        package_name = module["package_names"]["apk"]
        plugin_name = package_name.removeprefix("rsyslog-")
        if plugin_name not in plugin_entries:
            text = replace_once(
                text,
                r"(?m)^(\s*omotel(?:\s*:[^\n]+)?\s*)$",
                rf"\1\n\t{plugin_name}",
                "Alpine plugin package anchor",
            )
            plugin_entries.append(plugin_name)

        text = add_configure_flag(
            text,
            module["configure_flag"],
            r"(?m)^(\s*)--enable-omotel\s*\\$",
            "Alpine configure option anchor",
            "\t\t",
        )

    modules_by_name = {module["module"]: module for module in modules}
    profile_subpackages = " ".join(
        f"{profile['package_names']['apk']}:_profile_{profile['profile']}" for profile in profiles
    )
    if profile_subpackages not in text:
        text = replace_once(
            text,
            r'(?m)^(subpackages="[^"]*)"$',
            rf'\1 {profile_subpackages}"',
            "Alpine subpackages assignment",
        )
    for profile in profiles:
        function_name = f"_profile_{profile['profile']}()"
        if function_name not in text:
            text = text.rstrip() + "\n" + alpine_profile_function(profile, modules_by_name)

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
