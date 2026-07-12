# How To

New to RiftBuild? Start with the [Examples](Examples/) - a numbered series of
tiny projects you build and tinker with, from a one-line hello world up to
pulling and building a real third-party library and a playable video game.

[Reference.md](Reference.md) collects syntax notes on individual features.

## Syntax highlighting

Syntax files for `.build` and `.buildvars` files are provided for
VS Code, Vim and Emacs:

### VS Code setup
Install the extension from the `.vsix` file shipped in
[`Source/Resources`](../Source/Resources/):

1. Open the Extensions view (`Ctrl+Shift+X`).
2. Click the `...` menu at the top-right of the Extensions panel and choose
   **Install from VSIX...**
3. Pick `Source/Resources/riftbuild-0.6.4-beta.vsix` from this repository.

Or install it from the command line instead:

(run from the repository root), then reload any open VS Code windows.

```
code --install-extension "Source/Resources/riftbuild-0.6.4-beta.vsix"
```

### Vim setup
Copy [`Source/Resources/riftbuild.vim`](../Source/Resources/riftbuild.vim)
  into `~/.vim/syntax/` (`vimfiles\syntax\` on Windows), then associate the
  file type in your vimrc like so.

   ```vim
  " Associate .build files with riftbuild syntax
  augroup riftbuildFileType
      autocmd!
      autocmd BufNewFile,BufRead *.build,*.buildvars set filetype=riftbuild
  augroup END
   ```

### Emacs setup

Emacs is configured through an *init file* that it reads on startup.

1. Your init file is found here: `~/.emacs`. `~` is `%APPDATA%` on windows.

2. Create a folder for extra Lisp files inside your Emacs directory:
   `~/.emacs.d/lisp/`

3. Copy [`Source/Resources/riftbuild-mode.el`](../Source/Resources/riftbuild-mode.el)
   into that folder.

4. Add these two lines to the end of the init file:

   ```elisp
   (add-to-list 'load-path "~/.emacs.d/lisp/")
   (require 'riftbuild-mode)
   ```

5. Quit Emacs completely (`C-x C-c`) and start it again. Any `.build` or
   `.buildvars` file you open now uses `riftbuild-mode` with full
   highlighting - the mode line at the bottom of the window says
   `RiftBuild`.