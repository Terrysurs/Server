#pragma once

namespace Win32Utils
{
    // A simple aggregation of file descriptors and OS Handles to stdout and stderr, completely invalid by default.
    struct ConsoleState
    {
        int stdOutFileDescriptor = -1;
        int stdErrFileDescriptor = -1;
        HANDLE stdOutHandle = nullptr;
        HANDLE stdErrHandle = nullptr;
        // The identity of this state is embedded into the values of the handles.
        // File descriptor values will most likely be always new since they are created by calling _dup().
        friend bool operator==(const ConsoleState& lhs, const ConsoleState& rhs)
        {
            return lhs.stdErrHandle == rhs.stdErrHandle && lhs.stdOutHandle == rhs.stdOutHandle;
        }
    };

    /**
     * Offers the application the capability to redirect the console output and toggle visibitily of the console window.
     * The console redirection is dependent on a class conforming to the same API as the LocalNamedPipe (see
     * LocalNamedPipe.h). Theoretically, that could be implemented in terms of an anonymous pipe or socket as well.
     * Although controlling the console window visibility could be done in spite of the validity of that pipe object,
     * the reasoning behind this class assumes it won't make sense to hide the console if there is no other way to show
     * the warnings there printed. Thus, it must receive a valid pipe object in its constructor and take ownership
     * of it thru a move operation. This precondition is reinforced by an assert in the constructor body:
     *
     * ```cpp
     * assert(pipe.readHandle() != nullptr); // ofc only matters in debug.
     *```
     * Notice that STDIN was never touched. It could be, but it is outside of the scope of what's planned for this
     * application for now.
     *
     * == CLASS INVARIANTS ==
     * - An instance of this class only makes sense if a valid pipe object is passed thru constructor.
     * - Unique ownership of the pipe object is implied.
     * - If bIsRedirected than there is a valid previous console state to which this class should restore.
     * - redirectConsole and restoreConsole only makes sense if there is a valid Pipe object.
     * - Although possible, hiding and showing window makes no sense if redirecting the console is not possible.
     */
    template <class Pipe> class ConsoleService
    {
      private:
        Pipe redirectTo;
        ConsoleState previousConsoleState;
        bool bIsRedirected = false;
        HWND window_;

        ConsoleState consoleState() const
        {
            ConsoleState state;
            state.stdErrFileDescriptor = _dup(_fileno(stderr));
            state.stdErrHandle = GetStdHandle(STD_ERROR_HANDLE);
            state.stdOutFileDescriptor = _dup(_fileno(stdout));
            state.stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

            return state;
        }

        void applyConsoleState(ConsoleState& state, FILE* stderrStream, DWORD nStderrHandle, FILE* stdoutStream,
                               DWORD nStdoutHandle) const
        {
            // Overriding the std streams with the handle to the write end of the pipe. For sake of simplicity on
            // the consumer side, both stdout and stderr goes to the same file.
            if (state == consoleState()) {
                return;
            }
            fflush(stderrStream);
            fflush(stdoutStream);
            SetStdHandle(nStderrHandle, state.stdErrHandle);
            SetStdHandle(nStdoutHandle, state.stdOutHandle);
            [[maybe_unused]] auto res2 = _dup2(state.stdErrFileDescriptor, _fileno(stderrStream));
            [[maybe_unused]] auto res = _dup2(state.stdOutFileDescriptor, _fileno(stdoutStream));
        }

      public:
        // std::forward<Pipe>(pipe) is just a way to ensure pipe is moved instead of copied.
        ConsoleService(Pipe&& pipe) noexcept : redirectTo{std::forward<Pipe>(pipe)}
        {
            assert(redirectTo.readHandle() != nullptr);
            constexpr auto sleepFor = 100;
            // Attempting to find the console window is best-effort if it is not the old style console.
            // This has been tested for the new Windows Terminal. Other terminals out there may not fit.
            Sleep(sleepFor);
            window_ = FindWindow(L"CASCADIA_HOSTING_WINDOW_CLASS", DistributionInfo::WindowTitle.c_str());
            if (window_ == nullptr) {
                window_ = GetConsoleWindow();
            }
        }
        ~ConsoleService() = default;

        bool isRedirected() const
        {
            return bIsRedirected;
        }

        HWND window() const
        {
            return window_;
        }

        // Redirects the application console to the [redirectTo] pipe.
        // Since there is only one pipe instance in this service class, there is no sense in calling this method more
        // than once. A flag is used to ensure the operation happens only once until console is restored.
        // Call it without arguments to perform a real redirection of the console.
        nonstd::expected<HANDLE, std::wstring> redirectConsole(FILE* stderrStream = stderr,
                                                               DWORD nStderrHandle = STD_ERROR_HANDLE,
                                                               FILE* stdoutStream = stdout,
                                                               DWORD nStdoutHandle = STD_OUTPUT_HANDLE)
        {
            if (!bIsRedirected) {
                HANDLE handle = redirectTo.writeHandle();
                if (handle == nullptr) {
                    return nonstd::make_unexpected(L"Pipe failed to expose the write handle");
                }
                // The pipe class must guarantee that if the Handle is valid, the File descriptor is also valid. See
                // LocalNamedPipe.h.
                int file_descriptor = redirectTo.writeFileDescriptor();
                fflush(stderrStream);
                fflush(stdoutStream);
                // Saving the standard streams context before redirecting.
                previousConsoleState = consoleState();
                ConsoleState newConsoleState{file_descriptor, file_descriptor, handle, handle};

                applyConsoleState(newConsoleState, stderrStream, nStderrHandle, stdoutStream, nStdoutHandle);
                // No buffering seems mandatory for this situation. Tests with launching child apps reading from this
                // pipe didn't work otherwise.
                setvbuf(stderrStream, nullptr, _IONBF, 0);
                setvbuf(stdoutStream, nullptr, _IONBF, 0);
                redirectTo.closeWriteHandles();
                std::ios::sync_with_stdio();
                bIsRedirected = true;
            }
            return redirectTo.readHandle();
        }

        // Restores the application console from a previous redirection.
        // Call it without arguments to perform a real operation to the console.
        void restoreConsole(FILE* stderrStream = stderr, DWORD nStderrHandle = STD_ERROR_HANDLE,
                            FILE* stdoutStream = stdout, DWORD nStdoutHandle = STD_OUTPUT_HANDLE)
        {
            if (!bIsRedirected) {
                return;
            }
            // Restoring the std streams context.
            applyConsoleState(previousConsoleState, stderrStream, nStderrHandle, stdoutStream, nStdoutHandle);
            _close(previousConsoleState.stdErrFileDescriptor);
            _close(previousConsoleState.stdOutFileDescriptor);
            std::ios::sync_with_stdio();
            redirectTo.disconnect();
            bIsRedirected = false;
            // invalidating previous console state, so there is no where to restore to.
            previousConsoleState = ConsoleState{};
        }

        bool hideConsoleWindow() const
        {
            return ShowWindow(window_, SW_HIDE) != FALSE;
        }

        bool showConsoleWindow(HWND topWindow = nullptr) const
        {
            if (topWindow != nullptr && IsWindowVisible(topWindow) == TRUE) {
                if (auto res = Win32Utils::resize_to(window_, topWindow); res != 0) {
                    Helpers::PrintErrorMessage(HRESULT_FROM_WIN32(res));
                }
            }

            // If the window was previously visible, the return value is nonzero.
            // If the window was previously hidden, the return value is zero.
            return ShowWindow(window_, SW_SHOWNORMAL) == FALSE;
        }
    };

}