# Contributors and provenance

- Junki / sumiteru7 and Sophia: original project, Guard A/B, verified v1.1.0-alpha.1 Patch 2.5 baseline, integration and beta packaging.
- Gustavo Kimura (Nexus **KimuraDev**, GitHub **GustavoKimura**): minidump investigation on an affected Patch 2.5 system, separate 0x22CA442 / read 0x164 path analysis, Guard C callback, signature and diagnostic implementation.

Contribution: https://github.com/sumiteru7/VehicleExplosionCrashGuard/pull/1
Original commit: 67e0b2b23734b2c63264828922294a6ac14d7392
Original author/committer metadata is retained in the Git history. No squash or replacement author is used for that commit.

The beta uses the officially archived v1.1.0-alpha.1 A/B implementation, not the PR's older v1.0 base. The supplied Guard C callback is retained verbatim. The only hook-installation adjustment is a disabled creation / five-byte footprint check / enable sequence. C counters, last-event diagnostic data and cleanup follow the contribution.

Original MIT and third-party license notices remain intact. This attribution does not imply that the contributor has reviewed this integrated beta or endorsed any unrelated changes.
