# Bash completion for netra.

_netra_complete_interfaces()
{
    local interfaces=""

    if [[ -d /sys/class/net ]]; then
        interfaces="$(compgen -W "$(cd /sys/class/net && printf '%s\n' *)" -- "$cur")"
    fi

    COMPREPLY=($interfaces)
}

_netra_complete_pids()
{
    local pids=""

    pids="$(compgen -W "$(cd /proc 2>/dev/null && printf '%s\n' [0-9]* 2>/dev/null)" -- "$cur")"
    COMPREPLY=($pids)
}

_netra_complete_hosts()
{
    if declare -F _known_hosts >/dev/null; then
        _known_hosts -a "$cur"
        return
    fi

    COMPREPLY=()
}

_netra_words()
{
    local netra_bin="${NETRA_BIN:-netra}"

    if [[ -x ./bin/netra ]]; then
        netra_bin="./bin/netra"
    fi

    "$netra_bin" __complete "$@" 2>/dev/null
}

_netra()
{
    local cur prev command kind words

    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD - 1]}"

    if [[ $COMP_CWORD -eq 1 ]]; then
        words="$(_netra_words commands)"
        COMPREPLY=($(compgen -W "$words" -- "$cur"))
        return
    fi

    command="${COMP_WORDS[1]}"
    kind="$(_netra_words value-kind "$command" "$prev")"

    case "$kind" in
        pid)
            _netra_complete_pids
            return
            ;;
        interface)
            _netra_complete_interfaces
            return
            ;;
        host)
            _netra_complete_hosts
            return
            ;;
        count)
            COMPREPLY=($(compgen -W "1 2 3 4 5 10" -- "$cur"))
            return
            ;;
        table)
            COMPREPLY=($(compgen -W "main local default all" -- "$cur"))
            return
            ;;
    esac

    words="$(_netra_words options "$command")"
    COMPREPLY=($(compgen -W "$words" -- "$cur"))
}

complete -F _netra netra
