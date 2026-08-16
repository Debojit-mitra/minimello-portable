# Agent Rules for MiniMello Portable

## 1. Documentation Enforcement
Whenever you (the AI agent) implement a new feature, fix a bug, or resolve an issue in this codebase, you MUST document the change in the `DEVELOPER_NOTES.md` file located in the root directory.

### Guidelines for updating DEVELOPER_NOTES.md:
- Locate the "Development History & Changelog" section.
- NEVER add your entry to a version that is already released. Check `include/version.h` to see the currently released version.
- Add your entry under a future Unreleased version heading (e.g., `### v0.2.1 (Unreleased)`). If one does not exist, create it above the currently released version.
- Use the existing table format: `| Type | Description | Files Affected |`
- Valid Types include: `✨ Feature`, `🐛 Bugfix`, `🎨 UI/UX`, `⚙️ Config`, `🚀 Build`.
- Keep the description concise but highly informative for future reference.
