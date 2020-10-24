# Don't put duplicate lines in the history.
export HISTCONTROL=ignoredups

# Check the window size after each command and, if necessary, update the values of LINES and COLUMNS.
shopt -s checkwinsize

# Set prompt.
PS1="\w \$ "

# If this is an xterm set the title to user@host:dir
case "$TERM" in
xterm*|rxvt*)
    PROMPT_COMMAND='echo -ne "\033]0;${USER}@kiwi: ${PWD/$HOME/~}\007"'
    ;;
*)
    ;;
esac
