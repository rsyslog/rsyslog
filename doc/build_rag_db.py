
import os
import pickle
import json
import hashlib
import re
import traceback
from docutils import nodes

# Configuration
BUILD_DIR = "build/doctrees"
OUTPUT_FILE = "build/rag/rsyslog_rag_db.json"
MAX_CHUNK_CHARS = 2000
MIN_CHUNK_CHARS = 15

# Module prefixes for detecting module documentation pages
MODULE_PREFIXES = ('im', 'om', 'mm', 'pm', 'fm')

# Scope detection keywords
SCOPE_MAP = {
    "input parameter": "input",
    "module parameter": "module",
    "action parameter": "action",
    "parser parameter": "parser",
    "template parameter": "template",
}

# Generic headers that should not generate syntax templates
GENERIC_HEADERS = {
    "input parameters", "module parameters", "action parameters",
    "parameters", "configuration", "description", "notes", "examples",
    "usage", "see also", "caveats", "input usage", "statistic counters"
}

# Node type constants for cleaner isinstance checks
TEXT_NODES = (nodes.paragraph, nodes.Text, nodes.title)
CONTAINER_NODES = (
    nodes.bullet_list, nodes.enumerated_list, nodes.list_item,
    nodes.definition_list, nodes.definition, nodes.field_list, nodes.field,
    nodes.field_body, nodes.note, nodes.table, nodes.tgroup, nodes.tbody,
    nodes.row, nodes.entry, nodes.block_quote
)

def get_node_text(node):
    """Recursively extract raw text from a node, ignoring tags."""
    return node.astext().replace('\n', ' ').strip()

def generate_id(content, breadcrumbs):
    """Generate a stable unique ID for a chunk (not for security)."""
    seed = f"{breadcrumbs}|{content}".encode('utf-8')
    return hashlib.md5(seed, usedforsecurity=False).hexdigest()  # noqa: S324

def extract_module_from_filename(filename):
    """Extract rsyslog module name from filename."""
    base = os.path.basename(filename).split('.')[0]
    match = re.match(r'^([iofpm]m[a-z0-9]+)', base)
    if match:
        return match.group(1)
    return base

def is_module_page(filename):
    """Heuristic to determine if this file documents an rsyslog module."""
    base = os.path.basename(filename)
    return base.startswith(MODULE_PREFIXES)

def parse_attributes(context_stack, filename):
    """Extract module, scope, and item from the context stack."""
    module = extract_module_from_filename(filename)
    attributes = {
        "module": module if is_module_page(filename) else "core",
        "scope": "general",
        "item": "none"
    }
    for part in context_stack:
        if "Rsyslog Module" in part:
            found_mod = part.replace("Rsyslog Module ", "").strip()
            if found_mod:
                attributes["module"] = found_mod.split(' ')[0].rstrip(':')
                break
    stack_str = " ".join(context_stack).lower()
    for keyword, scope in SCOPE_MAP.items():
        if keyword in stack_str:
            attributes["scope"] = scope
            break
    if context_stack:
        attributes["item"] = context_stack[-1]
    return attributes

def get_syntax_template(attrs):
    """Generate a syntax hint based on scope and module."""
    module = attrs["module"]
    item = attrs["item"]
    scope = attrs["scope"]
    if not item or item.lower() in GENERIC_HEADERS or item == "none":
        return ""
    # Extract the actual parameter name from the reference or title
    # e.g., "param-imfile-file" -> "file", or "Input Parameters" -> ""
    match = re.search(r'(?:param-[a-z0-9]+-)?([a-z0-9\._]+)$', item, re.IGNORECASE)
    clean_item = match.group(1) if match else item.split()[-1]
    
    if scope == "input":
        return f'input(type="{module}" {clean_item}="...")'
    elif scope == "action":
        return f'action(type="{module}" {clean_item}="...")'
    elif scope == "module":
        return f'module(load="{module}" {clean_item}="...")'
    return ""

class ExtractorState:
    def __init__(self, filename):
        self.filename = filename
        self.chunks = []
        self.context_stack = []
        self.buffer = []
        self.buf_char_count = 0
        self.current_parent_id = None

    def flush_buffer(self):
        if not self.buffer:
            return
        
        text = "\n".join(self.buffer).strip()
        if len(text) < MIN_CHUNK_CHARS:
            return
            
        attrs = parse_attributes(self.context_stack, self.filename)
        breadcrumbs = " > ".join(self.context_stack)
        chunk_id = generate_id(text, breadcrumbs)
        
        # Determine if we can identify a specific item name from patterns
        if attrs["scope"] in ["input", "module", "action"]:
            match = re.search(r'param-[a-z0-9]+-([a-z0-9\._]+)', text, re.IGNORECASE)
            if match:
                attrs["item"] = match.group(1)
            elif attrs["item"].lower() in ["input parameters", "module parameters", "action parameters"]:
                words = text.split()
                if words and re.match(r'^[a-z0-9\._]+$', words[0], re.IGNORECASE):
                    attrs["item"] = words[0]

        syntax = get_syntax_template(attrs)
        syntax_line = f"Syntax: {syntax}\n" if syntax else ""
        
        vector_text = f"rsyslog {attrs['module']} {attrs['scope']} {attrs['item']} {text}"
        llm_text = f"Module: {attrs['module']}\nScope: {attrs['scope']}\n"
        if attrs['item'] != "none":
            llm_text += f"Item: {attrs['item']}\n"
        llm_text += syntax_line
        llm_text += f"Content: {text}"

        self.chunks.append({
            "chunk_id": chunk_id,
            "parent_id": self.current_parent_id,
            "type": "text_block",
            "attributes": attrs,
            "vector_text": vector_text,
            "llm_text": llm_text,
            "source": self.filename
        })
        self.buffer = []
        self.buf_char_count = 0

    def add_to_buffer(self, text):
        if not text: return
        self.buffer.append(text)
        self.buf_char_count += len(text)
        if self.buf_char_count >= MAX_CHUNK_CHARS:
            self.flush_buffer()

    def process_node(self, node):
        """Recursive node processor."""
        
        if isinstance(node, nodes.section):
            self.flush_buffer()
            title_node = next((n for n in node.children if isinstance(n, nodes.title)), None)
            old_parent_id = self.current_parent_id
            
            if title_node:
                title_text = get_node_text(title_node)
                self.context_stack.append(title_text)
                
                attrs = parse_attributes(self.context_stack, self.filename)
                breadcrumbs = " > ".join(self.context_stack[:-1])
                content = f"Section: {title_text} (Part of {self.context_stack[0]})"
                
                chunk_id = generate_id(content, breadcrumbs + " > " + title_text)
                self.chunks.append({
                    "chunk_id": chunk_id,
                    "parent_id": old_parent_id,
                    "type": "section_header",
                    "attributes": attrs,
                    "vector_text": f"rsyslog {attrs['module']} {attrs['scope']} {title_text}",
                    "llm_text": (f"Module: {attrs['module']}\nScope: {attrs['scope']}\nSection: {title_text}\n"
                                f"Content: Documentation for {title_text} in {self.context_stack[0]}."),
                    "source": self.filename
                })
                self.current_parent_id = chunk_id

            # Recurse into section children
            for child in node.children:
                if isinstance(child, nodes.title): continue
                self.process_node(child)
            
            self.flush_buffer()
            if title_node:
                self.context_stack.pop()
            self.current_parent_id = old_parent_id

        elif isinstance(node, nodes.literal_block):
            self.flush_buffer()
            code_text = node.astext()
            attrs = parse_attributes(self.context_stack, self.filename)
            chunk_id = generate_id(code_text, " > ".join(self.context_stack) + " [code]")
            self.chunks.append({
                "chunk_id": chunk_id,
                "parent_id": self.current_parent_id,
                "type": "code",
                "attributes": attrs,
                "vector_text": f"rsyslog {attrs['module']} {attrs['scope']} code example {self.context_stack[-1] if self.context_stack else ''}",
                "llm_text": f"Module: {attrs['module']}\nScope: {attrs['scope']}\nCode Example:\n{code_text}",
                "source": self.filename
            })

        elif isinstance(node, TEXT_NODES):
            # Direct text nodes contribute to buffer
            self.add_to_buffer(get_node_text(node))

        elif isinstance(node, CONTAINER_NODES):
            # Container nodes: recurse into children
            for child in node.children:
                self.process_node(child)
        
        elif isinstance(node, nodes.document):
            for child in node.children:
                self.process_node(child)
        
        else:
            # Fallback for unknown nodes: if they have children, try to recurse
            if hasattr(node, 'children'):
                for child in node.children:
                    self.process_node(child)

def process_doctree(filepath, filename):
    # Doctree files are generated by Sphinx during the same build; not untrusted data
    with open(filepath, 'rb') as f:
        try:
            doctree = pickle.load(f)  # noqa: S301
        except (pickle.UnpicklingError, EOFError, AttributeError) as e:
            print(f"Warning: Failed to process doctree {filepath}: {e}")
            return []
    
    state = ExtractorState(filename)
    
    # Context Injection
    title = "Untitled"
    for node in doctree.traverse(nodes.title):
        title = get_node_text(node)
        break
        
    if is_module_page(filename):
        module_name = extract_module_from_filename(filename)
        state.context_stack = [f"Rsyslog Module {module_name}"]
    else:
        state.context_stack = [title]

    state.process_node(doctree)
    state.flush_buffer()
    return state.chunks

def main():
    all_rag_data = []
    print(f"Scanning {BUILD_DIR}...")
    
    if not os.path.exists(BUILD_DIR):
        print(f"Error: Directory {BUILD_DIR} not found. Did you run 'make html'?")
        return

    for root, _, files in os.walk(BUILD_DIR):
        for file in files:
            if file.endswith(".doctree"):
                try:
                    file_chunks = process_doctree(os.path.join(root, file), file)
                    all_rag_data.extend(file_chunks)
                except Exception:
                    print(f"Failed to process {file}:")
                    traceback.print_exc()

    print(f"Generated {len(all_rag_data)} chunks.")
    
    output_dir = os.path.dirname(OUTPUT_FILE)
    os.makedirs(output_dir, exist_ok=True)

    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        json.dump(all_rag_data, f, indent=2)
    print(f"Saved to {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
