# Security policy

## Supported versions

The project is pre-release and currently contains planning documents and validation tooling. There is no stable engine or LTS branch. Security fixes target current development; handling is best effort with no response-time SLA.

## Private reports

Use [GitHub private vulnerability reporting](https://github.com/xsparc/omniweft/security/advisories/new) for suspected vulnerabilities in the repository, future engine or contribution workflows. Do not open a public issue containing an unpatched exploit, secret or private data. Repository setup must enable this feature; see [setup status](docs/REPOSITORY_SETUP.md).

Include affected commit/version, impact, reproduction steps, relevant configuration and a minimal safe fixture. Exclude credentials and unrelated personal data. The steward will assess reproducibility, coordinate a fix and agree appropriate disclosure through the advisory conversation where practical.

Relevant boundaries include untrusted world/assets, command authorization, plugin/provider execution, image/mesh parsers, resource exhaustion, persistence recovery and CI privileges. A provider running in a separate process is not automatically sandboxed. See [AI control](docs/AI_CONTROL_PROTOCOL.md) and [validation](docs/VALIDATION.md).
