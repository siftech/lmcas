# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ ccls, clang-tools, lib, neovim-unwrapped, neovimUtils, nixfmt, nodePackages
, python3Packages, rust-analyzer, rustfmt, vimPlugins, wrapNeovimUnstable, }:

let
  plugins = [
    {
      plugin = vimPlugins.ctrlp;
      config = ''
        let g:ctrlp_custom_ignore = '\v[\/]\.(git|hg|svn)$'
        let g:ctrlp_map = '<c-p>'
        let g:ctrlp_root_markers = ['.ctrlp_root']
        let g:ctrlp_working_path_mode = 'ra'
        let g:ctrlp_user_command = {
          \ 'types': {
            \ 1: ['.git', 'cd %s && git ls-files -co --exclude-standard'],
            \ },
          \ 'fallback': 'fd --search-path %s -t f'
          \ }
      '';
    }
    { plugin = vimPlugins.goyo-vim; }
    {
      plugin = vimPlugins.neoformat;
      config = ''
        augroup fmt
          autocmd!
          autocmd BufWritePre * Neoformat
        augroup END
        let g:neoformat_enabled_python = ['black']
        let g:neoformat_enabled_rust = ['rustfmt']
        let g:neoformat_rust_rustfmt = {
          \ 'exe': 'rustfmt',
          \ 'args': ['--edition', '2018'],
          \ 'stdin': 1,
          \ }
      '';
    }
    {
      plugin = vimPlugins.nerdtree;
      config = "noremap <tab> :NERDTreeToggle<cr>";
    }
    {
      plugin = vimPlugins.nvim-lspconfig;
      config = ''
        lua << EOF
            local nvim_lsp = require('lspconfig')
            local on_attach = function(client, bufNumber)
              vim.api.nvim_buf_set_option(bufNumber, 'omnifunc', 'v:lua.vim.lsp.omnifunc')

              local function buf_set_keymap(...)
                vim.api.nvim_buf_set_keymap(bufNumber, ...)
              end

              local opts = { noremap=true, silent=true }
              buf_set_keymap('n', '<leader>qf', '<cmd>lua vim.lsp.buf.code_action()<CR>', opts)
              buf_set_keymap('n', '<leader>rn', '<cmd>lua vim.lsp.buf.rename()<CR>', opts)
              buf_set_keymap('n', 'K', '<cmd>lua vim.lsp.buf.hover()<CR>', opts)
              buf_set_keymap('n', '[g', '<cmd>lua vim.diagnostic.goto_prev()<CR>', opts)
              buf_set_keymap('n', ']g', '<cmd>lua vim.diagnostic.goto_next()<CR>', opts)
              buf_set_keymap('n', 'gD', '<cmd>lua vim.lsp.buf.declaration()<CR>', opts)
              buf_set_keymap('n', 'gd', '<cmd>lua vim.lsp.buf.definition()<CR>', opts)

              if client.resolved_capabilities.document_highlight then
                vim.api.nvim_exec([[
                  hi LspReferenceRead cterm=bold ctermbg=yellow guibg=LightYellow
                  hi LspReferenceText cterm=bold ctermbg=yellow guibg=LightYellow
                  hi LspReferenceWrite cterm=bold ctermbg=yellow guibg=LightYellow
                  augroup lsp_document_highlight
                    autocmd!
                    autocmd CursorHold <buffer> lua vim.lsp.buf.document_highlight()
                    autocmd CursorMoved <buffer> lua vim.lsp.buf.clear_references()
                  augroup END
                ]], false)
              end
            end
            for _, lsp in ipairs({'ccls', 'pyright', 'rust_analyzer'}) do
              nvim_lsp[lsp].setup({ on_attach = on_attach })
            end
        EOF
      '';
    }
    { plugin = vimPlugins.quick-scope; }
    {
      plugin = vimPlugins.undotree;
      config = "noremap <m-tab> :UndotreeToggle<cr>";
    }
    {
      plugin = vimPlugins.vim-airline;
      config = ''
        let g:airline_powerline_fonts = 0
      '';
    }
    {
      plugin = vimPlugins.vim-airline-themes;
      config = ''
        let g:airline_theme='dark_minimal'
      '';
    }
    { plugin = vimPlugins.vim-fugitive; }
    { plugin = vimPlugins.vim-gitgutter; }
    { plugin = vimPlugins.vim-nix; }
    {
      plugin = vimPlugins.vim-startify;
      config = ''
        let g:startify_change_to_dir = 0
      '';
    }
  ];

  pluginsNormalized = map (x:
    if x ? plugin then
      { optional = false; } // x
    else {
      plugin = x;
      optional = false;
    }) plugins;
  pluginConfig = p:
    if (p.config or "") != "" then ''
      " ${p.plugin.pname or p.plugin.name} {{{
      ${p.config}
      " }}}
    '' else
      "";
  pluginRC = lib.concatMapStrings pluginConfig pluginsNormalized;

  config = neovimUtils.makeNeovimConfig {
    configure = {
      customRC = pluginRC + builtins.readFile ./extra-config.vim;
      packages.custom.start = map (p: p.plugin) plugins;
    };
  };

  extraPackages = [
    ccls
    clang-tools # clang-format
    nixfmt
    nodePackages.pyright
    python3Packages.black
    rust-analyzer
    rustfmt
  ];

in wrapNeovimUnstable neovim-unwrapped (config // {
  # wrapNeovimUnstable and makeNeovimConfig seem to disagree on the type of
  # wrapperArgs...
  wrapperArgs = lib.escapeShellArgs (config.wrapperArgs
    ++ [ "--suffix" "PATH" ":" (lib.makeBinPath extraPackages) ]);
})
