# Contributing to ThetaZ

## Branch naming policy

All branches must follow this format:

`Firstword-secondword-[optional-third...eighth]-devbranch`

Rules:

- First word must start with a capital letter and use only letters or numbers.
- Second word must use lowercase letters only.
- Words 3 through 8 are optional and must use lowercase letters only.
- Use hyphens (`-`) between words.
- Branch name must end with `-devbranch`.

Examples:

- Valid: `John-france-devbranch`
- Valid: `John-france-team-alpha-devbranch`
- Invalid: `John-France-devbranch`
- Invalid: `John-france-team-123-devbranch`

## Commit message policy

Every commit message must start with one of the following prefixes:

- `Fix:`
- `Feat:`
- `Docs:`

Examples:

- `Fix: handle nil branch name`
- `Feat: add commit message validation workflow`
- `Docs: add contributing guide`
