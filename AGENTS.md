# Metadata Isolation repository safety contract

Safety priorities: data integrity > correctness > reproducibility > simplicity
> performance > model/tool efficiency.

- No metadata writes during Physical Metadata Audit; only explicit user actions
  may open the physical Properties editor.
- Unsafe/unknown Virtual differences always block the whole virtual write.
- Known skip exceptions are limited to individually qualified cases.
- Do not create a release, tag or upload without explicit user authorization.
- GitHub Actions candidate hashes identify exact tested bytes; release never
  rebuilds or substitutes an untested binary.
- Native foobar2000 Win32/WTL, no forced WinUI/WPF dependency.
- Follow central `lxsdd/dev-infrastructure` standards:
  GITHUB-FIRST.md, CI-QUALIFICATION.md, RELEASE-POLICY.md,
  FOOBAR2000-COMPONENT.md, SHARED-DESKTOP-UX.md,
  UI-RUNTIME-QUALIFICATION.md and FOOBAR2000-NATIVE-DIALOGS.md.
- Production-path runtime/visual tests remain a separate qualification gate.