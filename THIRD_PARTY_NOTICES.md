# Third-party notices

This project fetches and links the following pinned dependencies during build. They are not maintained by this project and remain under their respective licenses.

| Component | Version | License | Upstream |
|---|---:|---|---|
| RED4ext.SDK | 1.0.0 | MIT License | https://github.com/WopsS/RED4ext.SDK |
| SafetyHook | v0.7.0 | Boost Software License 1.0 | https://github.com/cursey/safetyhook |
| Zydis | v4.1.0 | MIT License | https://github.com/zyantific/zydis |

SafetyHook v0.7.0 fetches Zydis v4.1.0 when `SAFETYHOOK_FETCH_ZYDIS=ON`, as configured by this project.

Copies of the applicable license texts are included in the `LICENSES` directory:

- `LICENSES/RED4ext.SDK-MIT.txt`
- `LICENSES/SafetyHook-BSL-1.0.txt`
- `LICENSES/Zydis-MIT.txt`
