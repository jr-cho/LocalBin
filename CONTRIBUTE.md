# How to Contribute:

### 0. Make sure that your git is connected to your github

Use an ssh key

### 1. Clone the Main Repository:

```
git@github.com:jr-cho/LocalBin.git
```
### 2. Create a New Branch for Each Task:

Always create a branch for your work instead of pushing to main:

```
git checkout -b feature/<short-description>
```

Example:
- feature/file-upload
- feature/authentication
- bugfix/upload-permissions
- refactor/config-system

### 3. Set Up Your Environment:

Install UV Project and Package Manager for Python.

#### i. Install Dependencies:
```
uv sync
```

### 4. Commit Your Changes:

```
git add .
git commit -m "Add file upload endpoint with checksum verification"
git push origin feature/file-upload // this is an example
```

### 5. Open a Pull Request:
1. Go the GitHub Repository
2. Click "Compare & pull request"
3. Ensure:
    - Base branch: `main`
    - Compare branchL: your feature branch
4. Write a clear description of your changes and submit

### 6. After the merge:
After the review and approval of the PR. Your branch gets merged into the `main` branch. Now delete the feature branch to keep the tree clean.

```
git branch -d feature/file-upload
git push origin --delete feature/file-upload
```

### 7. Sync with the latest code:

Before starting new work, always update your local repo:

```
git checkout main
git pull origin main
uv sync
```

If you’re mid branch and need to update it:

```
git fetch origin
git rebase origin/main
uv sync
```


