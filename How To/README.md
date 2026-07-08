# How To

New to RiftBuild? Start with the [Examples](Examples/) - a numbered series of
tiny projects you build and tinker with, from a one-line hello world up to
pulling and building a real third-party library and a playable video game.

[Reference.md](Reference.md) collects syntax notes on individual features.

## Syntax highlighting

Syntax files for `.build` and `.buildvars` files are provided for both
VS Code and Vim:

- **VS Code** - install the RiftBuild extension from
  [RiftBuild-VSCode](https://github.com/AliElSaleh/RiftBuild-VSCode)
  (or open that repo and run `code --install-extension <the .vsix file>`).
- **Vim** - copy [`Source/Resources/riftbuild.vim`](../Source/Resources/riftbuild.vim)
  into `~/.vim/syntax/` (`vimfiles\syntax\` on Windows), then associate the
  file type: `autocmd BufRead,BufNewFile .build,*.build,*.buildvars set filetype=riftbuild`
  in your vimrc.
