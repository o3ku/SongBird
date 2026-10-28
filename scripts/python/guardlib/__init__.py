"""Shared plumbing for the Python ports of scripts/check-*.ps1.

Why these exist at all: every gate in this repository was a PowerShell script, and
PowerShell is not installable on Debian any more (Microsoft's apt source is signed with
the same SHA1 self-signed key as its own metadata, and sqv rejects that since 2026-02-01).
The PR gate therefore ran none of them. These ports answer the same question in pure
standard-library Python, so the gate can run them without touching the OS trust store.

The PowerShell scripts stay: they are what Windows developers and the GitHub release job
run. A port is a second implementation of the same rule, which is a real maintenance
cost -- but a rule nobody executes in the PR gate is worth less than the cost.
"""
