#!/usr/bin/env python3
"""
AGRISCAN Full Setup Script
- Copies your repo files into ~/AI/knowledge/
- Creates workspaces in AnythingLLM via API
- Uploads and embeds all files automatically

BEFORE RUNNING:
  1. Open AnythingLLM Desktop
  2. Go to Settings → Developer API → generate an API key
  3. Paste it below as ANYTHINGLLM_API_KEY
"""

import os
import shutil
import requests
import time
from pathlib import Path

# ── CONFIG — edit these ───────────────────────────────────────────────────────
REPO_SOURCE          = Path.home() / "Documents" / "trials" / "agdb"
AI_ROOT              = Path.home() / "AI"
ANYTHINGLLM_API_KEY  = "JJN8JJF-EF1MSC2-GQ80MNN-WQXDJYY"   # ← paste your key here
ANYTHINGLLM_BASE_URL = "http://localhost:3001/api/v1"

WORKSPACES = {
    "agriscan-firmware": {
        "display": "AGRISCAN Firmware",
        "desc": "ESP32 firmware, sensors, C/C++ embedded code",
        "system_prompt": (
             "You are an embedded systems expert focused on ESP32-class MCUs, Arduino, "
             "C/C++ firmware, and low-power wireless sensor networks. You have full "
             "context of the AGRISCAN precision agriculture system, including the Hub, "
             "CropBands, sensors (soil moisture, DHT22, DS18B20, etc.), SD/SQLite "
             "logging, WiFi/AP behavior, and calibration/physics code.\n\n"
             "Core responsibilities:\n"
             "- Read and reason directly from the uploaded source files before answering. "
             "When unsure, search the codebase instead of guessing.\n"
             "- Help debug crashes, brownouts, calibration issues, timing/SD problems, "
             "and state-machine logic.\n"
             "- Propose concrete changes: show exact function names, signatures, and "
             "patch-style snippets that can be pasted into the repo.\n"
             "- Always explain the hardware–software interaction (e.g., power supply, "
             "brownout behavior, WiFi current spikes, SD writes, ADC use) when relevant.\n"
             "- When there are multiple options, compare tradeoffs briefly and then "
             "recommend one path for AGRISCAN’s current phase (prototype on a budget).\n\n"
             "Style:\n"
             "- Be opinionated but precise. Use short, implementation-ready answers.\n"
             "- Reference file paths and line numbers or symbols when possible.\n"
             "- If a question belongs more to hardware or dashboard, say so explicitly "
             "and suggest what to check there.\n"
             "When relevant, consult PROJECT_MASTER.md and session notes in this workspace to understand current architecture, open issues, and past decisions."
        ),
        "upload_repo": True,
    },
    "agriscan-hardware": {
        "display": "AGRISCAN Hardware",
        "desc": "Wiring, sensors, datasheets, calibration",
        "system_prompt": (
            "You are a hardware engineer for the AGRISCAN project, with expertise in "
            "ESP32/XIAO boards, 18650 Li-ion, MT3608 and similar regulators, sensor "
            "wiring, ADCs, and field-deployable low-power designs for agriculture.\n\n"
            "Core responsibilities:\n"
            "- Use uploaded schematics, notes, and datasheets to reason about power "
            "paths, current draw, brownouts, noise, and grounding.\n"
            "- Help design and debug: wiring diagrams, resistor dividers for battery "
            "sensing, decoupling/ bulk capacitors, sensor placement, and connector "
            "choices for real-field robustness.\n"
            "- Translate firmware symptoms (e.g., random resets, WiFi drops, SD issues, "
            "noisy readings) into likely electrical causes and concrete tests "
            "(multimeter checks, temporary jumpers, how to prototype a fix).\n"
            "- When suggesting circuits, keep them breadboard/proto-friendly and "
            "respect cost constraints; prefer minimal extra parts, and make it clear "
            "what is optional vs essential for pilots.\n\n"
            "Style:\n"
            "- Give step-by-step wiring guidance (which pin to which, typical values, "
            "and why) and simple measurement procedures.\n"
            "- When firmware changes are required to support the hardware idea, call "
            "that out explicitly for the firmware workspace.\n"
            "When relevant, consult PROJECT_MASTER.md and session notes in this workspace to understand current architecture, open issues, and past decisions."
        ),
        "upload_repo": False,
    },
    "agriscan-dashboard": {
        "display": "AGRISCAN Dashboard",
        "desc": "Frontend, backend, API, data visualization",
        "system_prompt": (
            "You are a full-stack engineer for the AGRISCAN dashboard. You work across "
            "HTML/CSS/JavaScript/TypeScript, APIs, SQLite/PostgreSQL-style SQL, and "
            "backend glue code.\n\n"
            "Core responsibilities:\n"
            "- Use the uploaded frontend and backend files to understand the existing "
            "engineering dashboard, REST endpoints (e.g., /api/current, /api/set_time, "
            "/api/diagnostics), and database schema.\n"
            "- Help design and debug data flows end-to-end: browser → API → database → "
            "visualization, including reconnect logic, timestamp handling, and error "
            "states when the Hub is offline or degraded.\n"
            "- Propose concrete code changes with clear snippets and file paths; keep "
            "JS/TS examples compatible with the existing stack.\n"
            "- When needed, reason about how firmware constraints (sampling interval, "
            "WiFi behavior, timestamp source) should shape the dashboard UX and API "
            "contracts.\n\n"
            "Style:\n"
            "- Prefer clear, incremental improvements: small endpoints, small UI "
            "changes, and simple DB migrations described in detail.\n"
            "- When the right fix belongs in firmware or hardware, say so and explain "
            "what assumption the dashboard should or should not make.\n"
            "When relevant, consult PROJECT_MASTER.md and session notes in this workspace to understand current architecture, open issues, and past decisions."
        ),
        "upload_repo": False,
    },
}

GOOD_EXTENSIONS = {
    ".ino", ".cpp", ".c", ".h",
    ".py", ".js", ".ts", ".tsx", ".jsx",
    ".html", ".css", ".sql",
    ".json", ".yaml", ".yml", ".toml",
    ".md", ".txt",
}

SKIP_DIRS = {
    ".git", "node_modules", "build", "dist", ".next",
    "__pycache__", ".venv", "venv", "env",
    "target", "out", ".cache", "coverage",
}

# ── HELPERS ───────────────────────────────────────────────────────────────────
def headers():
    return {
        "Authorization": f"Bearer {ANYTHINGLLM_API_KEY}",
        "Content-Type": "application/json",
        "accept": "application/json",
    }

def api_get(path):
    return requests.get(f"{ANYTHINGLLM_BASE_URL}{path}", headers=headers())

def api_post(path, data=None):
    return requests.post(f"{ANYTHINGLLM_BASE_URL}{path}", headers=headers(), json=data or {})

def check_anythingllm():
    try:
        r = api_get("/auth")
        if r.status_code == 200:
            return True
        elif r.status_code == 403:
            print("❌ API key rejected. Check AnythingLLM → Settings → Developer API.")
            return False
        else:
            print(f"❌ Unexpected response: {r.status_code}")
            return False
    except requests.exceptions.ConnectionError:
        print("❌ Can't reach AnythingLLM at localhost:3001.")
        print("   Make sure AnythingLLM Desktop is open and running.")
        return False

def create_workspace(slug, display_name, system_prompt):
    r = api_get(f"/workspace/{slug}")
    if r.status_code == 200:
        print(f"  ↩  '{display_name}' already exists, skipping.")
        return True

    r = api_post("/workspace/new", {"name": display_name})
    if r.status_code in (200, 201):
        print(f"  ✓  Created: {display_name}")
        time.sleep(0.5)
        api_post(f"/workspace/{slug}/update", {"openAiPrompt": system_prompt})
        return True
    else:
        print(f"  ❌ Failed to create '{display_name}': {r.status_code} {r.text}")
        return False

def upload_and_embed(file_path: Path, workspace_slug: str):
    upload_headers = {
        "Authorization": f"Bearer {ANYTHINGLLM_API_KEY}",
        "accept": "application/json",
    }
    try:
        with open(file_path, "rb") as f:
            r = requests.post(
                f"{ANYTHINGLLM_BASE_URL}/document/upload",
                headers=upload_headers,
                files={"file": (file_path.name, f, "text/plain")}
            )
    except Exception as e:
        print(f"    ✗ Error reading {file_path.name}: {e}")
        return False

    if r.status_code not in (200, 201):
        return False

    data = r.json()
    docs = data.get("documents", [])
    if not docs:
        return False

    location = docs[0].get("location")
    if not location:
        return False

    embed_r = api_post(
        f"/workspace/{workspace_slug}/update-embeddings",
        {"adds": [location]}
    )
    return embed_r.status_code in (200, 201)

def collect_repo_files(src: Path):
    files = []
    for root, dirs, filenames in os.walk(src):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
        for name in filenames:
            path = Path(root) / name
            if path.suffix.lower() in GOOD_EXTENSIONS:
                files.append(path)
    return files

def create_master_doc(workspace_path: Path, ws_info: dict):
    content = f"""# {ws_info['display']}
{ws_info['desc']}

---

## Current Architecture
<!-- How is this part of the system structured? -->

## Active Work
<!-- What are you currently building or debugging? -->

## Known Bugs / Issues
<!-- Open problems -->

## Key Decisions Made
<!-- Design/hardware/software choices and why -->

## Next Steps
<!-- What needs to happen next -->

## Session Log
<!-- Paste Claude chat summaries here — this is your living doc -->
| Date | Topic | Outcome |
|------|-------|---------|
|      |       |         |
"""
    doc_path = workspace_path / "PROJECT_MASTER.md"
    doc_path.write_text(content)
    return doc_path

def create_session_template(root: Path):
    content = """# Session Note — [DATE]

## Workspace
<!-- agriscan-firmware / agriscan-hardware / agriscan-dashboard -->

## Goal
<!-- What were you trying to do? -->

## Files touched
<!-- List files edited or created -->

## Decisions made
<!-- Design or hardware choices -->

## What worked / What got solved

## Blockers / Open questions

## Next steps

---
*After filling this out, upload it to the matching AnythingLLM workspace.*
"""
    template = root / "session-notes" / "SESSION_TEMPLATE.md"
    template.write_text(content)

# ── MAIN ──────────────────────────────────────────────────────────────────────
def main():
    print("\n🌱 AGRISCAN Full Setup")
    print("=" * 50)

    if ANYTHINGLLM_API_KEY == "YOUR_API_KEY_HERE":
        print("\n⚠️  Set your API key first!")
        print("   Edit this script → paste key into ANYTHINGLLM_API_KEY")
        print("   Get it from: AnythingLLM → Settings → Developer API\n")
        return

    if not REPO_SOURCE.exists():
        print(f"\n❌ Repo not found at: {REPO_SOURCE}\n")
        return
    print(f"\n✅ Repo found: {REPO_SOURCE}")

    print("\n🔌 Connecting to AnythingLLM...")
    if not check_anythingllm():
        return
    print("  ✅ Connected!")

    # Local folders
    print("\n📁 Creating local folders...")
    for ws_key in WORKSPACES:
        folder = AI_ROOT / "knowledge" / ws_key
        folder.mkdir(parents=True, exist_ok=True)
        print(f"  ✓ ~/AI/knowledge/{ws_key}/")
    (AI_ROOT / "session-notes").mkdir(parents=True, exist_ok=True)
    (AI_ROOT / "master-maps").mkdir(parents=True, exist_ok=True)
    create_session_template(AI_ROOT)

    # Master docs
    print("\n📝 Creating PROJECT_MASTER.md files...")
    master_docs = {}
    for ws_key, ws_info in WORKSPACES.items():
        ws_path = AI_ROOT / "knowledge" / ws_key
        doc_path = create_master_doc(ws_path, ws_info)
        master_docs[ws_key] = doc_path
        print(f"  ✓ {ws_key}/PROJECT_MASTER.md")

    # Create workspaces in AnythingLLM
    print("\n🗂  Creating AnythingLLM workspaces...")
    for ws_key, ws_info in WORKSPACES.items():
        create_workspace(ws_key, ws_info["display"], ws_info["system_prompt"])

    # Copy repo files locally
    print("\n📂 Scanning repo for files...")
    repo_files = collect_repo_files(REPO_SOURCE)
    print(f"  Found {len(repo_files)} files")

    for f in repo_files:
        rel = f.relative_to(REPO_SOURCE)
        dst = AI_ROOT / "knowledge" / "agriscan-firmware" / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, dst)

    # Upload + embed into AnythingLLM
    print(f"\n⬆️  Uploading + embedding into 'AGRISCAN Firmware'...")
    print("   This may take a minute...\n")

    success, fail = 0, 0
    all_files = repo_files + [master_docs["agriscan-firmware"]]

    for file_path in all_files:
        ok = upload_and_embed(file_path, "agriscan-firmware")
        if ok:
            print(f"  ✓ {file_path.name}")
            success += 1
        else:
            print(f"  ✗ {file_path.name}")
            fail += 1
        time.sleep(0.3)

    # Done
    print("\n" + "=" * 50)
    print(f"✅ Done! {success} files embedded, {fail} failed.\n")
    print("AnythingLLM workspaces ready:")
    for ws_info in WORKSPACES.values():
        print(f"  • {ws_info['display']}")
    print()
    print("Next steps:")
    print("  • Hardware + Dashboard workspaces are empty — add notes as you go")
    print("  • After Claude sessions, fill out SESSION_TEMPLATE.md and upload it")
    print("  • Your system prompts are already set per workspace")
    print()

if __name__ == "__main__":
    main()