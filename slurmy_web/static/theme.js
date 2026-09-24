'use strict';
const storedTheme = localStorage.getItem('slurmy-theme');
document.documentElement.dataset.theme = storedTheme ||
  (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
