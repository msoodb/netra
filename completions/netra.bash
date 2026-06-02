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

    if compopt +o default 2>/dev/null; then
        :
    fi

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

_netra()
{
    local cur prev command commands options

    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD - 1]}"

    commands="overview connections interfaces routes dns ping"

    case "$prev" in
        -p|--pid)
            _netra_complete_pids
            return
            ;;
        -i|--interface|--name)
            _netra_complete_interfaces
            return
            ;;
        -t|--target)
            _netra_complete_hosts
            return
            ;;
        -n|--count)
            COMPREPLY=($(compgen -W "1 2 3 4 5 10" -- "$cur"))
            return
            ;;
        -T|--table)
            COMPREPLY=($(compgen -W "main local default all" -- "$cur"))
            return
            ;;
    esac

    if [[ $COMP_CWORD -eq 1 ]]; then
        COMPREPLY=($(compgen -W "$commands -h --help" -- "$cur"))
        return
    fi

    command="${COMP_WORDS[1]}"
    case "$command" in
        overview)
            options="--json --csv --text -h --help"
            ;;
        connections)
            options="-p --pid -i --interface -l --listening -v --verbose --json --csv --text -h --help"
            ;;
        interfaces)
            options="-i --name -u --up -v --verbose --json --csv --text -h --help"
            ;;
        routes)
            options="-T --table -i --interface -v --verbose --json --csv --text -h --help"
            ;;
        dns)
            options="-v --verbose --json --csv --text -h --help"
            ;;
        ping)
            options="-t --target -n --count -v --verbose --json --csv --text -h --help"
            ;;
        *)
            options="$commands -h --help"
            ;;
    esac

    COMPREPLY=($(compgen -W "$options" -- "$cur"))
}

complete -F _netra netra
