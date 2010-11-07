# Don't put duplicate lines in the history.
export HISTCONTROL=ignoredups
# ...and ignore same sucessive entries.
export HISTCONTROL=ignoreboth

# Check the window size after each command and, if necessary,
# update the values of LINES and COLUMNS.
shopt -s checkwinsize

# Set the prompt.
export PS1="[\t] [ \u in \w ] \$ "
