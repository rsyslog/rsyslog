" ====================================================================
" Project‑local Vim settings for rsyslog
" (drop this file as .vimrc or .exrc in the project root)
" ====================================================================

" Use spaces, no tabs
setlocal expandtab
setlocal shiftwidth=4
setlocal softtabstop=4
setlocal tabstop=4

" Trim trailing whitespace on save
autocmd BufWritePre * :%s/\s\+$//e

" File‑type specific overrides:

" Makefiles always use actual tabs
autocmd FileType make setlocal noexpandtab

" Shell and autoconf scripts: 2‑space indent
autocmd FileType sh,autoconf setlocal expandtab shiftwidth=2 softtabstop=2 tabstop=2

" YAML/CI files: 2‑space indent
autocmd FileType yaml    setlocal expandtab shiftwidth=2 softtabstop=2 tabstop=2

" Python: 4‑space indent (already default above, but explicit here)
autocmd FileType python  setlocal expandtab shiftwidth=4 softtabstop=4 tabstop=4

