# Publishing Playing.haus

The site is static GitHub Pages hosting for `mxpf/uxn-emulator`, served from
the root of the `gh-pages` branch. The homepage lives at `/`, with games at
`/tiny-neighbors/` and `/no-escape/`, and Sketchpad at `/sketchpad/`.
Only generated browser assets belong there;
the editable source stays on `constellation-game`.

Before publishing an update, activate Emscripten 4.0.15 and run:

```sh
make runner-check garden-check square-check sketch-check playing-web garden-web-check square-web-check sketch-web-check
```

Serve `build/web/` locally and run the browser checks described in
[Tiny Neighbors](TINY-NEIGHBORS.md#browser-host) and [No Escape!](SQUARE-DEMO.md#browser-host-no-escape).
Also run the shared interface and homepage checks:

```sh
node tests/console_browser_check.cjs http://127.0.0.1:8766/
node tests/playinghaus_browser_check.cjs http://127.0.0.1:8766/
node tests/sketch_browser_check.cjs http://127.0.0.1:8766/sketchpad/
```

Commit the source, then copy the
contents of `build/web/` into a clean checkout of `gh-pages`, preserving its
`CNAME` file (`playing.haus`). Review and commit that artifact update and push
`gh-pages`. Do not force-push or publish source files, local recordings, or tests.
Use a fresh build directory when packaging, so obsolete generated files do not
leak into the release. `square-web` alone builds the new game, but `playing-web`
builds the complete site, both games and Sketchpad. Keep the test-only
`build/sketch-check/` and `build/runner-web/` directories out of the artifact.

Porkbun manages the domain. The intended DNS configuration is:

| Type | Host | Value |
| --- | --- | --- |
| ALIAS | @ | mxpf.github.io |
| CNAME | www | mxpf.github.io |

Preserve unrelated mail/TXT records. Domain-verification TXT records should
also remain in place. Configure the custom domain in GitHub Pages before
pointing DNS at GitHub. Once DNS and the certificate are ready, enforce HTTPS.
Do not add wildcard records.

Initial rollout (2026-09-08): GitHub built the static artifact and accepted
`playing.haus` as its custom domain. Porkbun's authoritative nameservers return
the GitHub apex addresses, the `www` CNAME and the GitHub ownership TXT record.
At handoff, the `.haus` parent zone had not yet published the new delegation,
so public resolution, GitHub ownership verification and HTTPS were still pending.
After propagation, finish verification at
`https://github.com/settings/pages_verified_domains/playing.haus`, enable
Enforce HTTPS in the repository's Pages settings, and run the browser check
against `https://playing.haus/tiny-neighbors/`, plus the homepage check against
`https://playing.haus/`. Do not bypass certificate errors during testing.

See [GitHub's custom-domain documentation](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site).
