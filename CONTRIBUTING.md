# How to Contribute

## Your First Pull Request
We use GitHub for our codebase. You can start by reading [How To Pull Request](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/about-pull-requests).

## Without Semantic Versioning
Development base on branch `master`. We promise the **Forward Compatibility** by adding new package directory with suffix `v2/v3` when code has break changes.

## Branch Organization
We use [git-flow](https://nvie.com/posts/a-successful-git-branching-model/) as our branch organization, as known as [FDD](https://en.wikipedia.org/wiki/Feature-driven_development)


## Bugs
### 1. How to Find Known Issues
We are using [Github Issues](https://github.com/bytedance/sonic/issues) for our public bugs. We keep a close eye on this and try to make it clear when we have an internal fix in progress. Before filing a new task, try to make sure your problem doesn’t already exist.

### 2. Reporting New Issues
Providing a reduced test code is a recommended way for reporting issues. Then can be placed in:
- Just in issues

### 3. Security Bugs
Please do not report the safe disclosure of bugs to public issues. Contact us by [Support Email](mailto:sonic@bytedance.com)

## Submit a Pull Request
Before you submit your Pull Request (PR) consider the following guidelines:
1. Search [GitHub](https://github.com/bytedance/sonic/pulls) for an open or closed PR that relates to your submission. You don't want to duplicate existing efforts.
2. Be sure that an issue describes the problem you're fixing, or documents the design for the feature you'd like to add. Discussing the design upfront helps to ensure that we're ready to accept your work.
3. [Fork](https://docs.github.com/en/github/getting-started-with-github/fork-a-repo) the bytedance/sonic repo.
4. In your forked repository, make your changes in a new git branch:
    ```
    git checkout -b fix/security_bug develop
    ```
5. Create your patch, including appropriate test cases.
6. Follow [Google C++ Style Guides](https://google.github.io/styleguide/cppguide.html).
7. Commit your changes using a descriptive commit message that follows [AngularJS Git Commit Message Conventions](https://docs.google.com/document/d/1QrDFcIiPjSLDn3EL15IJygNPiHORgU1_OOAqWjiDU5Y/edit).
   Adherence to these conventions is necessary because release notes will be automatically generated from these messages.
8. Push your branch to GitHub:
    ```
    git push origin fix/security_bug
    ```
9. In GitHub, send a pull request to `sonic:master`

Note: you must use one of `optimize/feature/fix/doc/ci/test/refactor` following a slash(`/`) as the branch prefix.

Your pr title and commit message should follow https://www.conventionalcommits.org/.

## Contribution Prerequisites
- You need fully checking with lint tools before submit your pull request.
- You are familiar with [Github](https://github.com) 
- Maybe you need familiar with [Actions](https://github.com/features/actions)(our default workflow tool).

## Code Style Guides
See [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
