; Emacs mode for editing ELC (Extended Lambda Calculus) files
;
; To use this mode, copy this file into your ~/.xemacs directory and add the following to init.el:
;
; (add-to-list 'load-path "~/.xemacs/")
; (require 'elc-mode)
; (add-to-list 'auto-mode-alist '("\\.elc\\'" . elc-mode))
;
; This mode has been tested with XEmacs 21.4.18
;
; See http://two-wugs.net/emacs/mode-tutorial.html for details on how to create/edit language modes
;
; Note: Emacs also uses the ".elc" extension for compiled versions of .el (Emacs Lisp) files -
; these are unrelated to the ELC language supported by nreduce.

(defvar elc-font-lock-keywords
  (list
   '(" +\\(\\(!?[A-Za-z0-9_-]+ +\\)+\\)="
     1 font-lock-variable-name-face)
   '("\\(^[A-Za-z0-9_-]+\\) +.*="
     1 font-lock-type-face)
   '("\(\\([A-Za-z0-9:_-]+\\)"
     1 font-lock-keyword-face)
   '("^[A-Za-z0-9_-]+ "
     . font-lock-keyword-face)
  '("\\<letrec\\|in\\>"
     . font-lock-keyword-face)))

(defvar elc-mode-hook nil)

(defvar elc-mode-syntax-table
  (let ((elc-mode-syntax-table (make-syntax-table)))
  ; This is added so entity names with underscores can be more easily parsed
	(modify-syntax-entry ?_ "w" elc-mode-syntax-table)
	; Comment styles are same as C++
	(modify-syntax-entry ?/ ". 1456" elc-mode-syntax-table)
	(modify-syntax-entry ?* ". 23" elc-mode-syntax-table)
	(modify-syntax-entry ?\n "> b" elc-mode-syntax-table)
	elc-mode-syntax-table)
  "Syntax table for elc-mode")

(defun elc-indent-line ()
  "Indent current line as ELC code."
  (interactive)
  (beginning-of-line)
  (if (bobp)
      (indent-line-to 0)                ; First line is always non-indented
    (if (looking-at "^[ \t]*in")
        (indent-line-to (- (* (buffer-syntactic-context-depth) default-tab-width) 1))
      (indent-line-to (* (buffer-syntactic-context-depth) default-tab-width)))))

(defun elc-mode ()
  (interactive)
  (kill-all-local-variables)
  (set-syntax-table elc-mode-syntax-table)
  ;; Set up font-lock
  (set (make-local-variable 'font-lock-defaults) '(elc-font-lock-keywords))
  ;; Register our indentation function
  (set (make-local-variable 'indent-line-function) 'elc-indent-line)  
  (setq major-mode 'elc-mode)
  (setq mode-name "ELC")
  (run-hooks 'elc-mode-hook)
  (turn-on-font-lock))

(provide 'elc-mode)
