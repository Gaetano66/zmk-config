# Corne · Tastenbelegung

[PDF öffnen](Keybindings.pdf) — eine A4-Seite mit Basis, Lower, Raise, Lighting und Combos. Die Anzeige geht von einem deutschen QWERTZ-Hostlayout aus und zeigt bei unbekannten Bindings die ZMK-Aktion im Original. Maßgeblich ist `config/corne.keymap`; die PDF liest die aktiven Bindings, nicht die Diagramme in den Kommentaren.

## Automatische Aktualisierung

Der mitgelieferte Pre-Commit-Hook ist in diesem Checkout aktiviert. In einem neuen Checkout einmal im Repo ausführen:

```sh
uv venv .venv
uv pip install --python .venv/bin/python -r scripts/requirements-keybindings.txt
git config --local core.hooksPath .githooks
```

Bei Änderungen an der Keymap oder den Pomodoro-Befehlsnummern erzeugt der Hook die PDF aus dem **Git-Index** und nimmt sie in denselben Commit auf. Auch Änderungen am Generator lösen eine Prüfung aus. Unveränderte Belegungen erzeugen keine neue PDF-Revision. Web-Änderungen und Commits mit `--no-verify` umgehen den lokalen Hook.

Manuell aktualisieren: `.venv/bin/python scripts/keybindings_pdf.py`.
Aktualität prüfen: `.venv/bin/python scripts/keybindings_pdf.py --check`.
