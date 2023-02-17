# rofi-cmd

run command-line applications and explore their output with Rofi

This reruns a command over and over again as the user types into the input field and lets the user choose from the results.

# usage

```
$ rofi-cmd [command] [result command]
```

The default command is `echo %s` and the default result command is `cat`. These are useless but benign.

If the command contains a directive like `%s`, Rofi's input field is placed at the directive. If not, the input field is used as its standard input.
Use POSIX directive like `%1$s` to insert it more than once (see `man fprintf`). These aren't validated, so can crash Rofi if you're not careful.
Use `%%` for a literal `%`.

Selecting the first entry of the results will make the result the entire stdout of the command; otherwise, it will only be the selected line.

If the result command contains a directive like `%s`, the selected entry is placed at the directive `%s`. If not, the entry is used as its standard input.

# examples

Calculator (with `genius`):

```
$ rofi-cmd genius
```

A more verbose calculator (with `qalc`):

```
$ rofi-cmd 'qalc "%s"'
```

Calculator but the results go to the clipboard (with `genius`, `xclip`):

```
$ rofi-cmd genius xclip
$ rofi-cmd genius | xclip  # functionally equivalent
```

Explore the current directory (with `ls`):

```
$ rofi-cmd 'ls %s'
```

Search through a file  (with `grep`):

```
$ rofi-cmd 'grep -in "%s" <the file>'
```

Switch windows (with `wmctrl`, `grep`, `cut`):

```
$ wmctrl -ia `rofi-cmd 'wmctrl -l | grep "%s"' 'cut -z -d " " -f1'`
```

Figure out how to write a printf directive (with `tcc`):

```
$ rofi-cmd "echo -e '#include <stdio.h>\nint main() { printf(%s); return 0; }' | tcc -run -"
```

Convert to hex in different ways (with `printf`, `python`, `tcc`):

```
$ rofi-cmd "printf 0x%%x %s"
$ rofi-cmd "python -c 'print(hex(%s))'"
$ rofi-cmd "echo -e '#include <stdio.h>\nint main() { printf(\"0x%%x\", %s); return 0; }' | tcc -run -"
```

# TODO

Figure out some way to get back to an empty input line. This is a challenge, though,
since the input proprocessing hook doesn't get called when the input field is empty,
and there is no obvious way to check the field's value outside of that hook.
It might have to wait until this gets fixed in Rofi.
