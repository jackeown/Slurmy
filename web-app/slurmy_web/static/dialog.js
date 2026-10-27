'use strict';

// Shared async dialogs: text is always inserted as text, never HTML.
window.SlurmyDialog = (() => {
  const dialog = document.querySelector('#app-dialog');
  const form = dialog.querySelector('form');
  const title = dialog.querySelector('#app-dialog-title');
  const message = dialog.querySelector('#app-dialog-message');
  const field = dialog.querySelector('#app-dialog-field');
  const label = dialog.querySelector('#app-dialog-input-label');
  const input = dialog.querySelector('#app-dialog-input');
  const error = dialog.querySelector('#app-dialog-error');
  const actions = dialog.querySelector('#app-dialog-actions');
  const cancel = dialog.querySelector('#app-dialog-cancel');
  const close = dialog.querySelector('#app-dialog-close');
  const rememberField = dialog.querySelector('#app-dialog-remember-field');
  const remember = dialog.querySelector('#app-dialog-remember');
  let queue = Promise.resolve();

  function open(options) {
    return new Promise(resolve => {
      const previousFocus = document.activeElement;
      let answer = null;
      const finish = value => { answer = value; dialog.close(); };
      rememberField.hidden = !options.remember; remember.checked = false;
      title.textContent = options.title;
      message.textContent = options.message || '';
      message.hidden = !options.message;
      dialog.classList.toggle('destructive', Boolean(options.danger));
      field.hidden = !options.input;
      input.value = options.input?.value || '';
      input.maxLength = options.input?.maxLength || 100;
      label.textContent = options.input?.label || 'Name';
      error.textContent = ''; error.hidden = true; input.removeAttribute('aria-invalid');
      cancel.textContent = options.cancelLabel || 'Cancel';
      cancel.hidden = options.cancel === false;
      actions.replaceChildren();
      for (const choice of options.choices) {
        const button = document.createElement('button');
        button.type = 'submit'; button.value = choice.value; button.textContent = choice.label;
        button.className = choice.danger ? 'danger' : choice.secondary ? 'secondary' : '';
        actions.append(button);
      }
      const submit = event => {
        event.preventDefault();
        const choice = event.submitter?.value || options.choices[0].value;
        if (options.input) {
          const value = input.value.trim();
          const problem = !value ? 'Enter a name.' : value.length > input.maxLength
            ? `Use at most ${input.maxLength} characters.` : options.input.validate?.(value);
          if (problem) {
            error.textContent = problem; error.hidden = false;
            input.setAttribute('aria-invalid', 'true'); input.focus(); return;
          }
          finish(value);
        } else finish(options.remember ? {value: choice, remember: remember.checked} : choice);
      };
      const cancelRequest = event => { event?.preventDefault(); finish(null); };
      const backdrop = event => {
        if (event.target !== dialog) return;
        const bounds = dialog.getBoundingClientRect();
        if (event.clientX < bounds.left || event.clientX > bounds.right ||
            event.clientY < bounds.top || event.clientY > bounds.bottom) cancelRequest();
      };
      const closed = () => {
        form.removeEventListener('submit', submit);
        dialog.removeEventListener('cancel', cancelRequest);
        dialog.removeEventListener('click', backdrop);
        cancel.removeEventListener('click', cancelRequest);
        close.removeEventListener('click', cancelRequest);
        if (previousFocus?.isConnected) previousFocus.focus();
        resolve(answer);
      };
      form.addEventListener('submit', submit);
      dialog.addEventListener('cancel', cancelRequest);
      dialog.addEventListener('click', backdrop);
      cancel.addEventListener('click', cancelRequest);
      close.addEventListener('click', cancelRequest);
      dialog.addEventListener('close', closed, {once: true});
      dialog.showModal();
      if (options.input) { input.focus(); input.select(); }
      else if (!cancel.hidden) cancel.focus();
      else actions.querySelector('button').focus();
    });
  }
  function choose(options) {
    // Multiple clicks never stack dialogs or resolve the wrong operation.
    const result = queue.then(() => open(options));
    queue = result.catch(() => {});
    return result;
  }
  return {
    choose,
    text: options => choose({...options, choices: [{value: 'accept', label: options.acceptLabel || 'Save'}]}),
    inform: options => choose({...options, cancel: false, choices: [{value: 'ok', label: 'Got it'}]}),
  };
})();
