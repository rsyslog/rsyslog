# Commenting Style Guide

This document defines how we use Doxygen comments in the project. It is optimized for AI tools like Codex and human contributors alike.

---

# Doxygen Commenting Philosophy: Emphasize Self-Documenting Code

The primary goal of these guidelines is to create **useful and maintainable documentation** without cluttering the codebase with redundant information. The core principle is that code should be written to be as **clear and self-explanatory as possible**. Comments should supplement the code, explaining intent, complexity, and side effects that are not obvious from reading the code itself.

## 1. When Doxygen Comments are Mandatory

These components are critical for understanding a module's architecture and public interface. They should always have full Doxygen comments.

* **File Headers**: Every `.c` and `.h` file should start with a `@file` block that describes the purpose of the module contained within the file.
* **Public API Functions**: Any function declared in a header file (`.h`) that is intended for use by other modules must be fully documented.
* **Public Data Structures**: All structs, enums, and typedefs exposed in header files must be documented. This includes a description of the structure's purpose and a brief explanation for each member.
* **Complex Functions**: Any function, public or static, whose implementation is non-trivial. Indicators of complexity include:
* Complex algorithms or logic.
* Subtle or non-obvious side-effects.
* Interaction with multiple other components or systems.
* Performance-critical code where specific implementation choices were made for optimization.
* A large number of parameters, where the purpose of each is not immediately obvious from its name.

## 2. When to AVOID Doxygen Comments

For "obvious" functions, the function's signature and implementation should be sufficient documentation. Adding a Doxygen block here adds noise and increases the maintenance burden, as the comments must be kept in sync with the code.

Do not add Doxygen blocks to the following types of functions, provided they are static:

* **Simple Getters/Setters**: A function that does nothing more than return or set a member of a struct.
* **Trivial Helper Functions**: Small, static functions with a clear, descriptive name that perform a single, simple operation.
* **Boilerplate Functions**: Functions that simply implement a required interface from the `rsyslog` module template with no special logic.

## 3. The "Obvious Function" Litmus Test

Before skipping a Doxygen comment on a function, ask these questions. If the answer to **all** of them is "yes," it's a good candidate to leave undocumented.

* Is the function **static**? (Public functions must always be documented).
* Does the **function name clearly and unambiguously describe what it does**?
* Is the function **short** (e.g., fewer than 15 lines)?
* Are the **purpose and type of all parameters clear from their names**?
* Does it have **no non-obvious side effects**?

## Proposed Workflow for Transformation

1.  **Prioritize the Public Interface**: First, ensure all `.h` files and their corresponding public functions and structs in `.c` files are fully documented.
2.  **Document Complex Static Functions**: Go through each `.c` file and add Doxygen comments to complex static functions and data structures.
3.  **Actively Skip Simple Functions**: Consciously apply the "Obvious Function" test and skip adding Doxygen blocks to trivial, static helper functions. If a function seems to need a long comment to explain what it does, first consider if it can be refactored to be more clear.
4.  **Review and Refine**: After a file is processed, review it to see if the balance feels right. Is it easy to grasp the purpose of the undocumented functions? If not, they may not be as "obvious" as initially thought, and a brief Doxygen comment might be warranted.

---
