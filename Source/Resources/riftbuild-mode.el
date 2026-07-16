;;; riftbuild-mode.el --- Major mode for RiftBuild build files -*- lexical-binding: t; -*-

;; Copyright (c) Artisan Softworks
;; Licensed under the BSD 3-Clause License. See the LICENSE file for details.

;; Author: Ali El Saleh
;; Version: 0.6.6-beta
;; Keywords: languages
;; URL: https://github.com/AliElSaleh/RiftBuild

;;; Commentary:

;; Syntax highlighting for RiftBuild build files (.build / .buildvars).
;; Matches the syntax accepted by Source/Parse.c.
;;
;; Install: copy this file somewhere on your `load-path', then add to your
;; init file:
;;
;;   (require 'riftbuild-mode)
;;
;; Files named .build, *.build and *.buildvars open in this mode
;; automatically.

;;; Code:

(defconst riftbuild--reserved-keys-re
  (concat
   "^\\s-*"
   "\\(?:Assembly\\|Extension\\|Type\\|SourceFiles\\|SourceDirector\\(?:y\\|ies\\)"
   "\\|BuildDirectory\\|IntermediateDirectory\\|Compiler\\|Linker\\|Assembler"
   "\\|Resource\\|Archiver\\|Libraries\\|Library\\|Apple\\|Icon\\|PCH\\|Bundle"
   "\\|Info\\.plist\\|Version\\.plist\\|TitleName\\|InternalName\\|Description"
   "\\|CompanyName\\|Copyright\\|Version\\|License\\|AlwaysRebuild\\(?:All\\)?\\)"
   "\\(?:\\.[A-Za-z0-9_.]+\\)?\\_>")
  "Keys reserved by RiftBuild (see ReservedKeys in Source/Parse.c).")

(defconst riftbuild--hooks-re
  (concat
   "^\\s-*\\(?:Pre\\|Post\\)"
   "\\(?:Depend\\|Build\\|Compile\\(?:AllFiles\\|File\\)?\\|Link\\)"
   "\\(?:\\.\\(?:Cmd\\|Exec\\(?:ute\\)?\\|Command\\|Copy\\|Move\\|NewDir\\(?:ectory\\)?"
   "\\|NewFile\\|WriteFile\\|AppendFile\\|Rename\\|Delete\\|Log\\|Wait\\|Sleep"
   "\\|Download\\|Unzip\\|Extract\\|Zip\\|Archive\\|InstallPackages?\\)\\)?\\_>")
  "Build phase hooks and their file-operation verbs.")

;; Keys written inside a Key { } block nest under that namespace
;; (PreBuild { InstallPackage } == PreBuild.InstallPackage), so bare keys
;; inside hook / reserved-namespace blocks highlight like their dotted forms.

(defconst riftbuild--hook-block-header-re
  (concat
   "^\\s-*\\(\\(?:Pre\\|Post\\)"
   "\\(?:Depend\\|Build\\|Compile\\(?:AllFiles\\|File\\)?\\|Link\\)\\)"
   "\\(?::[^ \t\n{#]+\\)*\\s-*{?\\s-*$")
  "A Pre*/Post* hook opening a { } block (verbs written as nested keys).")

(defconst riftbuild--hook-verbs-re
  (concat
   "^\\s-*\\(?:Cmd\\|Exec\\(?:ute\\)?\\|Command\\|Copy\\|Move\\|NewDir\\(?:ectory\\)?"
   "\\|NewFile\\|WriteFile\\|AppendFile\\|Rename\\|Delete\\|Log\\|Wait\\|Sleep"
   "\\|Download\\|Unzip\\|Extract\\|Zip\\|Archive\\|InstallPackages?\\)\\_>")
  "A bare file-operation verb on its own line inside a hook block.")

(defconst riftbuild--reserved-block-header-re
  (concat
   "^\\s-*\\(Assembly\\|Compiler\\|Linker\\|Assembler\\|Resource\\|Archiver"
   "\\|Library\\|Apple\\|PCH\\|Bundle\\|License\\)"
   "\\(?::[^ \t\n{#]+\\)*\\s-*{?\\s-*$")
  "A reserved namespace opening a { } block (sub-keys written as nested keys).
Value-list keys (SourceFiles, Libraries, ...) are excluded on purpose: their
blocks hold values, not sub-keys.")

(defun riftbuild--block-limit ()
  "Return the position of the } closing the { } block after point.
Used as the anchored-highlighter limit for block header matches."
  (save-excursion
    (if (re-search-forward "^\\s-*}" nil t) (point) (point-max))))

(defconst riftbuild-font-lock-keywords
  `(;; build phase hooks + file-operation verbs
    (,riftbuild--hooks-re . font-lock-preprocessor-face)
    ;; bare verbs nested inside a Pre*/Post* { } block
    (,riftbuild--hook-block-header-re
     (1 font-lock-preprocessor-face)
     (,riftbuild--hook-verbs-re (riftbuild--block-limit) nil
      (0 font-lock-preprocessor-face)))
    ;; bare sub-keys nested inside a reserved namespace { } block
    (,riftbuild--reserved-block-header-re
     (1 font-lock-type-face)
     ("^\\s-*[A-Za-z_][A-Za-z0-9_.]*" (riftbuild--block-limit) nil
      (0 font-lock-type-face)))
    ;; Assert.* keys and any key ending in .ErrorMessage
    ("^\\s-*Assert\\.[A-Za-z0-9_.]*" . font-lock-preprocessor-face)
    ("[A-Za-z0-9_.]*\\.ErrorMessage\\_>" . font-lock-preprocessor-face)
    ;; options and presets
    ("^\\s-*option\\.[A-Za-z0-9_.]*" . font-lock-preprocessor-face)
    ("^\\s-*\\(?:preset\\|default\\.options\\)\\_>" . font-lock-preprocessor-face)
    ;; directives: .Run .Stop .Abort .Help
    ("^\\s-*\\.\\(?:run\\|stop\\|abort\\|help\\)\\_>" . font-lock-preprocessor-face)
    ;; dependencies
    ("^\\s-*Depends?\\_>" . font-lock-preprocessor-face)
    ;; reserved keys
    (,riftbuild--reserved-keys-re . font-lock-type-face)
    ;; control / condition keywords
    ("\\_<\\(?:if\\|else\\|import\\|include\\|or\\|contains\\|starts_with\\|ends_with\\)\\_>"
     . font-lock-keyword-face)
    ;; version comparison operators: v== v> v< v>= v<=
    ("\\_<v\\(?:==\\|>=\\|<=\\|>\\|<\\)" . font-lock-builtin-face)
    ;; built-in variables: %_DirectoryName, %_Date.Year, %_Platform, ...
    ("[%$@]_[A-Za-z0-9_.]*" . font-lock-builtin-face)
    ;; variable references: %Name $Name @Name, %{Name} %(Name) %-(Name)
    ("[%$@]-?\\(?:{[A-Za-z0-9_.-]+}\\|([A-Za-z0-9_.-]+)\\|[A-Za-z0-9_.]+\\)"
     . font-lock-constant-face)
    ;; comparison / condition symbols and the backtick value reset
    ("==\\|!=\\|<=\\|>=\\|[<>=!|^;`]" . font-lock-builtin-face)
    ;; any other key at the start of a line (user-defined)
    ("^\\s-*[A-Za-z_][A-Za-z0-9_.]*" . font-lock-variable-name-face))
  "Font lock rules for `riftbuild-mode'.")

(defvar riftbuild-mode-syntax-table
  (let ((st (make-syntax-table)))
    (modify-syntax-entry ?#  "<" st)  ; comment to end of line
    (modify-syntax-entry ?\n ">" st)
    (modify-syntax-entry ?\" "\"" st) ; quoted strings
    (modify-syntax-entry ?_  "_" st)  ; keys are dotted symbols
    (modify-syntax-entry ?.  "_" st)
    ;; backslash is a path separator in values, not an escape
    (modify-syntax-entry ?\\ "." st)
    (modify-syntax-entry ?%  "." st)
    (modify-syntax-entry ?$  "." st)
    (modify-syntax-entry ?@  "." st)
    (modify-syntax-entry ?-  "." st)
    st)
  "Syntax table for `riftbuild-mode'.")

(defconst riftbuild--block-start-re
  (concat
   "^\\s-*\\(?:"
   "\\.help"
   "\\|[A-Za-z0-9_.]*\\.errormessage"
   ;; PreX.WriteFile ... or a bare WriteFile verb nested inside a hook block
   "\\|\\(?:\\(?:pre\\|post\\)\\(?:depend\\|build\\|compile\\|link\\)\\.\\)?"
   "\\(?:writefile\\|appendfile\\)\\(?:\\s-+[^{#\n]*?\\)?"
   "\\)\\s-*\\(?:\\({\\)\\s-*\\)?$")
  "Start of a .Help / .ErrorMessage / WriteFile heredoc block.
Group 1 is the opening brace when it sits on the same line.")

(defconst riftbuild--syntax-propertize-comments
  (syntax-propertize-rules
   ;; \# is an escaped hash (not a comment) and \" is an escaped quote
   ;; (not a string delimiter)
   ("\\\\\\([\"#]\\)" (1 "."))
   ;; ## opens/closes a block comment; the second # must not start a
   ;; line comment of its own after the closing pair
   ("\\(#\\)\\(#\\)" (1 "!") (2 "."))
   ;; a lone # comments to end of line - consume it so a ## later on the
   ;; same line is not mistaken for a block comment delimiter
   ("#[^#\n].*$" (0 (ignore))))
  "Comment-related syntax rules (# to end of line, ## ... ## blocks).")

(defun riftbuild--syntax-propertize (start end)
  "Apply syntax properties between START and END."
  (funcall riftbuild--syntax-propertize-comments start end)
  ;; .Help / .ErrorMessage / heredoc blocks: mark the surrounding braces as
  ;; generic string delimiters so their contents fontify as plain text
  (let ((case-fold-search t))
    (goto-char start)
    (while (re-search-forward riftbuild--block-start-re end t)
      (let ((brace (match-beginning 1)))
        (unless brace
          ;; the { may sit alone on the following line
          (forward-line 1)
          (when (looking-at "\\s-*\\({\\)\\s-*$")
            (setq brace (match-beginning 1))))
        (when brace
          (put-text-property brace (1+ brace) 'syntax-table
                             (string-to-syntax "|"))
          (goto-char (1+ brace))
          (when (re-search-forward "^\\s-*\\(}\\)" nil t)
            (put-text-property (match-beginning 1) (match-end 1)
                               'syntax-table (string-to-syntax "|"))))))))

;;;###autoload
(define-derived-mode riftbuild-mode prog-mode "RiftBuild"
  "Major mode for editing RiftBuild build files."
  (setq-local comment-start "#")
  (setq-local comment-start-skip "#+\\s-*")
  (setq-local comment-end "")
  (setq-local font-lock-defaults '(riftbuild-font-lock-keywords nil t))
  (setq-local font-lock-multiline t)
  (setq-local syntax-propertize-function #'riftbuild--syntax-propertize))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.build\\(?:vars\\)?\\'" . riftbuild-mode))

(provide 'riftbuild-mode)

;;; riftbuild-mode.el ends here
