# Publishing Playing.haus

The site is static GitHub Pages hosting for `mxpf/uxn-emulator`, served from
the root of the `gh-pages` branch. Only generated browser assets belong there;
the editable source stays on `constellation-game`.

Before publishing an update, activate Emscripten 4.0.15 and run:

```sh
make garden-check garden-web-check
```

Serve `build/web/` locally and run the browser checks described in
[Tiny Neighbors](TINY-NEIGHBORS.md#browser-host). Commit the source, then copy the
contents of `build/web/` into a clean checkout of `gh-pages`, preserving its
`CNAME` file (`playing.haus`). Review and commit that artifact update and push
`gh-pages`. Do not force-push or publish source files, local recordings, or tests.

Porkbun manages the domain. The intended DNS configuration is:

| Type | Host | Value |
| --- | --- | --- |
| A | @ | 185.199.108.153 |
| A | @ | 185.199.109.153 |
| A | @ | 185.199.110.153 |
| A | @ | 185.199.111.153 |
| CNAME | www | mxpf.github.io |

Preserve unrelated mail/TXT records. Domain-verification TXT records should
also remain in place. Configure the custom domain in GitHub Pages before
pointing DNS at GitHub. Once DNS and the certificate are ready, enforce HTTPS.
Do not add wildcard records.

See [GitHub's custom-domain documentation](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site).
