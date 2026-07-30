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
3. Pick `Source/Resources/riftbuild-0.6.10-beta.vsix` from this repository.

Or install it from the command line instead:

(run from the repository root), then reload any open VS Code windows.

```
code --install-extension "Source/Resources/riftbuild-0.6.10-beta.vsix"
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

## Double-clicking .build files (Windows)

Associating the `.build` extension with `riftbuild.exe` lets you build a
project straight from Explorer: double-click a `.build` file and riftbuild
runs in that file's folder, shows the build output, then waits for a key
press before the window closes.

### The quick way

1. Right-click any `.build` file and choose **Open with** >
   **Choose another app**.
2. Pick **Look for another app on this PC** (hidden under *More apps* on
   some Windows versions) and browse to your `riftbuild.exe`.
3. Make sure **Always use this app to open .build files** is checked when
   you confirm.

### The full setup - file icon and right-click context Rebuild/Clean

Registering the file type by hand additionally gives `.build` files the
RiftBuild icon and adds **Rebuild** and **Clean** entries to their
right-click context menu. Paste these into a Command Prompt (`cmd.exe`) - the keys
are per-user, so no administrator rights are needed. Change the first line to
wherever your `riftbuild.exe` lives. (If you save them as a `.bat` file
instead of pasting them, write every `"%1"` as `"%%1"` - batch files eat
single percent signs.)

```bat
set "RIFT=C:\Tools\riftbuild.exe"
reg add "HKCU\Software\Classes\.build" /ve /d "RiftBuild.Script" /f
reg add "HKCU\Software\Classes\RiftBuild.Script" /ve /d "RiftBuild Script" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\DefaultIcon" /ve /d "\"%RIFT%\",0" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\shell\open" /ve /d "Build" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\shell\open\command" /ve /d "\"%RIFT%\" \"%1\"" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\shell\rebuild" /ve /d "Rebuild" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\shell\rebuild\command" /ve /d "\"%RIFT%\" \"%1\" rebuild" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\shell\clean" /ve /d "Clean" /f
reg add "HKCU\Software\Classes\RiftBuild.Script\shell\clean\command" /ve /d "\"%RIFT%\" \"%1\" clean" /f
```

Notes:

- If `.build` files were previously associated with another program, Windows
  remembers that choice and it wins over the keys above. Either do the
  quick-way steps once and pick RiftBuild, or clear the remembered choice:

  ```bat
  reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.build" /f
  ```
- If the new icon doesn't show up right away, restart Explorer or sign out
  and back in.

To undo everything:

```bat
reg delete "HKCU\Software\Classes\.build" /f
reg delete "HKCU\Software\Classes\RiftBuild.Script" /f
```