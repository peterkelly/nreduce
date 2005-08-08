function update() {
  var stylesheet = window.top.frames['main'].document.styleSheets[0];
  var rules = stylesheet.rules; // IE

  if (!rules) // everything else
    rules = stylesheet.cssRules;

  var len = rules.length;

  for (i = 0; i < len; i++) {
    var rule = rules[i];
    var name = rule.selectorText.substring(1);
    var input = document.getElementById(name);

    if (input != null) {
      if (input.checked)
        rule.style.display = '';
      else
        rule.style.display = 'none';
    }
  }
}

window.top.frames['top'].update = update;
