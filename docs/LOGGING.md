# Using logforge

A guide to the whole library: the interface, the macros, the backends, and how to
replace an existing logging layer with it.

- [1. The shape of a log line](#1-the-shape-of-a-log-line)
- [2. The interface](#2-the-interface)
- [3. Where a line goes](#3-where-a-line-goes)
- [4. Call-site macros](#4-call-site-macros)
- [5. The legacy names](#5-the-legacy-names)
- [6. Backends](#6-backends)
- [7. Writing a backend](#7-writing-a-backend)
- [8. Levels and configuration text](#8-levels-and-configuration-text)
- [9. Migrating an existing logging layer](#9-migrating-an-existing-logging-layer)

## 1. The shape of a log line

```
LOGFORGE_LOG_INFO("pad {} refused {}", name, count)
   │
   │ 1. is the current logger interested in info?
   │    no  -> the arguments are never touched
   ▼
   │ 2. render: "pad in refused 3"
   ▼
Logger::log(Record{info, "pad in refused 3", 17, component, file, line})
   │
   │ 3. the backend: one fwrite, one spdlog sink set, or your own
   ▼
somewhere
```

Three properties come out of that order:

- **Filtering is free.** A disabled statement costs one virtual call, so a hot
  path may pass an expensive expression without guarding it.
- **Rendering happens once**, in the front end, and the backend receives text.
  A backend never parses a format string.
- **A backend only writes.** That is the whole job, which is why replacing one is
  a link-time decision rather than a refactor.

## 2. The interface

```cpp
namespace logforge {

enum class Level { trace, debug, info, warn, error, critical, off };

struct Record {
    Level level;
    const char *text;       // borrowed
    std::size_t size;       // an embedded zero is a normal byte
    const char *component;  // borrowed, optional
    const char *file;       // borrowed, optional
    int line;               // 0 when unknown
};

class Logger {
public:
    virtual ~Logger() {}
    virtual bool enabled(Level level) const = 0;
    virtual void log(const Record &record) = 0;
};
}
```

**The contract.** `log()` is synchronous. When it returns, the caller may free the
text and the tags, so a backend that wants to keep either must copy it. That one
rule is why the interface needs no ownership story, no `shared_ptr` and no
allocation on the producer's side.

**Why text and not a structured record.** Not every producer has a format string
and an argument list to hand: a runtime concatenates a component name with a
message, a stream parser has already run something through `operator<<`. An
interface that demanded "format plus arguments" would force those producers to
keep their own renderer and re-render, which is the duplication this library
exists to remove.

**Helper functions.** `log_to()` saves the brace noise:

```cpp
logforge::log_to(logger, logforge::Level::warn, text);                 // std::string
logforge::log_to(logger, logforge::Level::warn, data, size);           // view
logforge::log_to(logger, logforge::Level::warn, data, size, "udp");    // + component
```

They require a non-null `logger`; the macros check that for you.

## 3. Where a line goes

Resolution is per thread, first match wins:

1. the innermost `ScopedBinding` on this thread;
2. the process default installed with `set_default()`;
3. `nullptr` -- "nobody is listening", and the statement is skipped.

```cpp
logforge::set_default(&backend);                    // process-wide

void Pad::dispatch() {
    const logforge::ScopedBinding binding(logger_);  // this thread, this scope
    LOGFORGE_LOG_DEBUG("tick {}", tick_);
}                                                   // previous logger restored
```

- **Nesting** works: each binding restores whatever was current when it was built.
- **Binding `nullptr`** does not mean silence, it means "no thread-specific
  logger", so the process default still applies. To silence a scope on purpose,
  bind a `NullLogger`.
- **Threads.** The binding is `thread_local`; one thread's binding is invisible to
  another. A binding is per *translation unit group*, not per process: if the same
  code is loaded into two shared libraries, each has its own copy of the
  thread-local slot. Link logforge into one module (the executable) if that
  matters.
- **Lifetime.** A backend installed with `set_default()` must outlive every thread
  that can log. Install it first, uninstall it (`set_default(nullptr)`) and join
  the workers before destroying it.

## 4. Call-site macros

| Macro | Where it goes |
|---|---|
| `LOGFORGE_LOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL(...)` | the current logger |
| `LOGFORGE_LOG(level, ...)` | the current logger, level as an expression |
| `LOGFORGE_LOG_AT(logger, level, ...)` | that logger, ignoring the binding |
| `LOGFORGE_LOG_TAGGED(tag, level, ...)` | the current logger, with a component |
| `LOGFORGE_LOG_AT_TAGGED(logger, tag, level, ...)` | both |

The logger and the level are each evaluated exactly once, even though both are
used twice. The tag is a `const char *` that must outlive the statement -- pass
`name.c_str()` for a `std::string`.

### Format specifiers

| Text | Result |
|---|---|
| `{}` | the argument as text |
| `{:4}` | at least four characters; numbers pad on the left, text on the right, and a wider value is never cut |
| `{:x}` `{:X}` | an integer in hex, lower or upper case |
| `{:04x}` | hex padded with zeros to four characters |
| `{:.2f}` | two digits after the point |
| `{:7.2f}` | width and precision together |
| `{{` `}}` | one brace |

Two rules keep this safe to call from anywhere:

- **A specifier that does not apply is ignored.** `{:x}` on a string prints the
  string; `{:.2f}` on an integer prints the integer.
- **Anything else is copied verbatim.** A malformed specifier, a width above 512,
  a precision above 64, a message with fewer arguments than braces -- the text
  comes out whole, with the parts that could not be filled left visible. Nothing
  throws, and nothing is truncated.

That is deliberate: fmt raises `format_error` on the same input, and a logging
call inside an event handler is the last place that may unwind.

Floating point defaults to the shortest of 15 and 17 significant digits that
reads back as the same value -- the spelling the JSON writer uses. It agrees with
fmt on ordinary values and can differ in the last digit of a value that needs all
17 digits.

### Argument types

Text (`const char*`, `char*`, arrays, `std::string`), `char`, `bool`, every integer
width, every floating point type, and pointers (printed as `0x...`). A type with
no log text is a compile error that asks for a cast -- the same thing spdlog
requires of an enum.

### Source location

`__FILE__` and `__LINE__` are captured by default and reach the backend as
`Record::file`/`Record::line`. Define `LOGFORGE_SOURCE_LOCATION=0` for a whole
target to remove them from the binary:

```cmake
target_compile_definitions(app PRIVATE LOGFORGE_SOURCE_LOCATION=0)
```

## 5. The legacy names

```cpp
#include <logforge/compat/legacy_names.hpp>

LOG_INFO("element {} started on port {}", name, port);
```

`LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERR` and `LOG_CRITICAL` are
available, and nothing more is required of a file that already uses them.

This header is **not** part of `<logforge/logforge.hpp>` on purpose. Those names
belong to glog, syslog and several embedded frameworks; a library that defines
them from its umbrella header starts a quiet war with whatever the including
project already uses. The guards are `#ifndef`, so the winner would depend on
include order -- the one failure mode that produces no diagnostic at all. Include
this header deliberately, in the files that need it.

`LOG_ERR` reports at error and `LOG_CRITICAL` at critical, which are two different
levels.

## 6. Backends

### `StderrLogger`

```cpp
logforge::StderrLogger logger(logforge::Level::info);
```

No sinks, no configuration, one `fwrite()` per record -- whole lines under
concurrency without a mutex, and a line that survives a crash because stderr is
unbuffered. Use it for a library's own tests and for the window between startup
and the real backend.

### `SpdlogBackend`

```cpp
logforge::SpdlogBackend::Config config;
logforge::SpdlogBackend backend(config);
logforge::set_default(&backend);
```

| Field | Meaning |
|---|---|
| `level` | minimum level; `Level::off` silences everything |
| `console` | a colour sink on stdout |
| `file_path` | empty: no file |
| `max_file_bytes` | 0: one file that never rotates |
| `max_files` | rotation depth |
| `udp_host`, `udp_port` | empty host or zero port: no UDP sink |
| `pattern` | spdlog pattern for every sink; empty keeps spdlog's default |

A configuration that ends up with no sink at all falls back to the console rather
than accepting records and dropping them.

`component` becomes a `"component: "` prefix on the message body -- exactly what
`StderrLogger` writes -- so changing backends changes where a line goes and how
the pattern decorates it, not what it says.

`backend.native()` returns the `spdlog::logger`, which is the escape hatch for
named loggers, custom sinks and spdlog's backtrace buffer. `backend.flush()`
and `backend.set_pattern()` are the two operations a program needs often enough to
deserve a wrapper.

Constructing a backend can throw `spdlog_ex` (an unwritable path, for example).
That happens at startup, in one place, which is the right time and place for it.

## 7. Writing a backend

Implement two functions:

```cpp
class MyBackend : public logforge::Logger {
public:
    bool enabled(logforge::Level level) const override {
        return level != logforge::Level::off && level >= min_level_;
    }

    void log(const logforge::Record &record) override {
        // Write record.text[0 .. record.size) somewhere, now.
    }
};
```

- `enabled()` must be cheap and thread safe: it runs on every statement.
- `log()` must not outlive its arguments, and must not recurse into the same
  backend from inside `log()` -- the sinks that do not deadlock will interleave.
- `Level::off` is a filter setting, not a severity: `enabled(Level::off)` must be
  false whatever the configured minimum is.

## 8. Levels and configuration text

`to_string(Level)` gives the canonical spelling; `parse_level(text, out)` reads one
back. Matching is case insensitive and the whole string must match, so a typo
fails instead of guessing:

| Accepted | Level |
|---|---|
| `trace` | trace |
| `debug` | debug |
| `info` | info |
| `warn`, `warning` | warn |
| `error`, `err` | error |
| `critical`, `fatal` | critical |
| `off` | off |

`out` is only written on success, so a caller keeps its default without saving and
restoring it:

```cpp
logforge::Level level = config_default_level;   // stays put on a typo
logforge::parse_level(json["level"].c_str(), level);
```

## 9. Migrating an existing logging layer

### From a pipeforge-style interface

| Before | After |
|---|---|
| `pipeforge::ILogger` | `logforge::Logger` |
| `pipeforge::LogLevel` | `logforge::Level` |
| `pipeforge::current_logger()` | `logforge::current()` |
| `pipeforge::set_current_logger(p)` | `logforge::ScopedBinding b(p)` |
| `pipeforge::set_default_logger(p)` | `logforge::set_default(p)` |
| `pipeforge::detail::LogBinding` | `logforge::ScopedBinding` |
| `pipeforge::StderrLogger` | `logforge::StderrLogger` |
| `pipeforge::SpdlogLogger` (adapter) | `logforge::SpdlogBackend` |
| `PF_LOG_INFO(...)` | `LOGFORGE_LOG_INFO(...)` |
| `PF_LOG_TO(logger, level, ...)` | `LOGFORGE_LOG_AT(logger, level, ...)` |
| `format_log(fmt, args...)` | `format_message(fmt, args...)` |

The important part is that `ScopedBinding` and the macros agree on the same
thread-local slot. Moving the macros without moving the binding produces code
that compiles and sends every per-object line to the process default.

### From raw spdlog

```cpp
// before
auto logger = spdlog::rotating_logger_mt("app", path, size, count);
logger->set_level(spdlog::level::info);
logger->info("pad {} refused {}", name, count);

// after
logforge::SpdlogBackend::Config config;
config.file_path = path;
config.max_file_bytes = size;
config.max_files = count;
config.level = logforge::Level::info;
logforge::SpdlogBackend backend(config);
logforge::set_default(&backend);
LOGFORGE_LOG_INFO("pad {} refused {}", name, count);
```

Keep spdlog directly if a feature is genuinely spdlog's -- named loggers per
module, a sink of your own, the backtrace buffer. `native()` is there so that
using one does not force the rest of the code to know about it.

### From an adapter that already holds a formatted string

This is the case that made the interface text-only. A subsystem that composes
messages with `operator<<` has no argument list to hand over, so it should not be
turned inside out -- it should hand over the text:

```cpp
// before: an adapter that re-rendered into a second string
void Bridge::forward(const TheirRecord &record) {
    LOG_INFO("[{}] {}", record.component, record.message);   // formats again
}

// after: straight through, no second format
void Bridge::forward(const TheirRecord &record) {
    logforge::log_to(logforge::current(), to_level(record.level),
                     record.message.data(), record.message.size(),
                     record.component.c_str(), record.file, record.line);
}
```
