# Don't put duplicate lines in the history.
export HISTCONTROL=ignoredups
# ...and ignore same sucessive entries.
export HISTCONTROL=ignoreboth

# Check the window size after each command and, if necessary,
# update the values of LINES and COLUMNS.
shopt -s checkwinsize

# Set the prompt.
set_ps1() {
	local COLOUR_NORMAL="\[\033[0;39m\]"
	local COLOUR_BLUE="\[\033[1;34m\]"
	local COLOUR_WHITE="\[\033[1;37m\]"

	PS1="${COLOUR_WHITE}[${COLOUR_BLUE}\t${COLOUR_WHITE}] [ ${COLOUR_BLUE}\u${COLOUR_WHITE} ${COLOUR_WHITE}in ${COLOUR_BLUE}\w ${COLOUR_WHITE}] \$${COLOUR_NORMAL} "
}
set_ps1
unset set_ps1

# If this is an xterm set the title to user@host:dir
case "$TERM" in
xterm*|rxvt*)
	PROMPT_COMMAND='echo -ne "\033]0;${USER}@kiwi: ${PWD/$HOME/~}\007"'
	;;
*)
	;;
esac
