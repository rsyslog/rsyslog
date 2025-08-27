import os
import re

# Extensions to consider
EXTS = {
    ".c", ".h", ".sh", ".rst", ".py", ".conf", ".txt", ".md",
    ".ac", ".am", ".y", ".l", ".pem", ".xml"
}

# Safe replacements (pattern -> replacement). Use word boundaries where safe.
REPLACEMENTS = [
    (r"\bhealt\b", "health"),
    (r"\bdepreceated\b", "deprecated"),
    (r"\benchance\b", "enhance"),
    (r"\bautomic\b", "atomic"),
    (r"\bpathes\b", "paths"),
    (r"\bnecessariy\b", "necessary"),
    (r"\bbootup\b", "boot up"),
    (r"\bfiter\b", "filter"),
    (r"\bstraighted\b", "straightened"),
    (r"\bsomes\b", "some"),
    (r"\bruning\b", "running"),
    (r"\bwhish\b", "wish"),
    (r"\bthuis\b", "thus"),
    (r"\bfaile\b", "failed"),
    (r"(?<=\s)wil(?=\s)", "will"),
    (r"\bcaling\b", "calling"),
    (r"\bony\b", "only"),
    (r"\btast\b", "task"),
    (r"\bspecif\b", "specify"),
    (r"\bpackging\b", "packaging"),
    (r"\bnempty\b", "empty"),
    (r"\bpreceeding\b", "preceding"),
    (r"\bachive\b", "achieve"),
    (r"\binout\b", "input"),
    (r"\bneeed\b", "need"),
    (r"\brecognice\b", "recognize"),
    (r"\bonces\b", "once"),
    (r"\bloger\b", "logger"),
    (r"\bths\b", "the"),
    (r"\bcommen\b", "comment"),
    (r"\bnumbe\b", "number"),
    (r"\bpersits\b", "persists"),
    (r"\bPerists\b", "Persists"),
    (r"\btransation\b", "transaction"),
    (r"\brealy\b", "really"),
    (r"\bdestructer\b", "destructor"),
    (r"\bdedected\b", "detected"),
    (r"\bdiscared\b", "discarded"),
    (r"\bhappend\b", "happened"),
    (r"\bther\b", "there"),
    (r"\bVLUES\b", "VALUES"),
    (r"\btiem\b", "time"),
    (r"\bsould\b", "should"),
    (r"\bDonn\b", "Done"),
    (r"\bwroth\b", "worth"),
    (r"\bcommends\b", "comments"),
    (r"\bmedias\b", "media"),
    (r"\blates\b", "latest"),
]


def iter_files(root: str):
    for dirpath, dirnames, filenames in os.walk(root):
        for name in filenames:
            _, ext = os.path.splitext(name)
            if ext.lower() in EXTS or name == "ChangeLog":
                yield os.path.join(dirpath, name)


def read_text_any(path: str):
    # Try utf-8 first, fall back to latin-1
    with open(path, "rb") as f:
        data = f.read()
    try:
        return data.decode("utf-8"), "utf-8"
    except UnicodeDecodeError:
        return data.decode("latin-1"), "latin-1"


def write_text(path: str, text: str, encoding: str):
    with open(path, "wb") as f:
        f.write(text.encode(encoding, errors="strict"))


def main():
    root = os.path.dirname(__file__)
    changed = 0
    patterns = [(re.compile(pat), rep) for pat, rep in REPLACEMENTS]
    for fp in iter_files(root):
        try:
            content, enc = read_text_any(fp)
        except Exception:
            continue
        new_content = content
        for rx, rep in patterns:
            new_content = rx.sub(rep, new_content)
        if new_content != content:
            try:
                write_text(fp, new_content, enc)
                changed += 1
            except Exception:
                pass
    print(f"changed_files={changed}")


if __name__ == "__main__":
    main()


