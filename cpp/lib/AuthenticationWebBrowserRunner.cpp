#include "AuthenticationWebBrowserRunner.hpp"
#include "AuthenticatorOAuth.hpp"
#include "../logger/SFLogger.hpp"

#include <memory>

#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef _WIN32
#include <Shellapi.h>
#endif

#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace Snowflake
{
    namespace Client
    {
        std::unique_ptr<IAuthenticationWebBrowserRunner> IAuthenticationWebBrowserRunner::instance
            = std::make_unique<AuthenticationWebBrowserRunner>();

        IAuthenticationWebBrowserRunner* IAuthenticationWebBrowserRunner::getInstance()
        {
            return instance.get();
        }

        void IAuthenticationWebBrowserRunner::setInstance(std::unique_ptr<IAuthenticationWebBrowserRunner> testInstance)
        {
            instance = std::move(testInstance);
        }

        void AuthenticationWebBrowserRunner::startWebBrowser(const std::string& ssoUrl)
        {
            CXX_LOG_TRACE("sf::AuthenticationWebBrowserRunnerL::startWebBrowser::%s", maskOAuthSecret(ssoUrl).c_str());

#ifdef __APPLE__
            CFURLRef urlRef = CFURLCreateWithBytes(
                NULL,                        // allocator
                (UInt8*)ssoUrl.c_str(),     // URLBytes
                ssoUrl.length(),            // length
                kCFStringEncodingASCII,      // encoding
                NULL                         // baseURL
            );
            OSStatus ret = LSOpenCFURLRef(urlRef, 0);
            CFRelease(urlRef);
            if (ret != noErr)
            {
                CXX_LOG_ERROR("sf::AuthenticationWebBrowserRunner::startWebBrowser::Failed to start web browser. err: %d", ret);
                throw AuthException("SFOAuthError" + std::to_string(ret));
            }
#elif _WIN32
            HINSTANCE ret = ShellExecuteA(NULL, "open", ssoUrl.c_str(), NULL, NULL,
                SW_SHOWNORMAL);
            if (ret > (HINSTANCE)32)
            {
                // success
                return;
            }
            CXX_LOG_ERROR("sf::AuthenticationWebBrowserRunner::startWebBrowser::Failed to start web browser. err: %d", (int)(unsigned long long)ret);
            throw AuthException("SFOAuthError " + std::to_string((int)(unsigned long long)ret));
#else
            // use fork to avoid using system() call and prevent command injection
            char* argv[3];
            pid_t child_pid;
            int child_status;
            argv[0] = const_cast<char*>("xdg-open");
            argv[1] = const_cast<char*>(ssoUrl.c_str());
            argv[2] = NULL;

            child_pid = fork();
            if (child_pid < 0)
            {
                // fork failed
                CXX_LOG_ERROR("sf::AuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser on fork. err: %s", strerror(errno));
                throw AuthException(std::string("SFOAuthError ") + strerror(errno));
            }
            else if (child_pid == 0)
            {
                // This is done by the child process.
                execvp(argv[0], argv);
                /* If execvp returns, it must have failed. */
                exit(-1); // Do nothing as we are in child process
            }
            else
            {
                // This is run by the parent. Wait for the child to terminate.
                if (waitpid(child_pid, &child_status, 0) < 0)
                {
                    CXX_LOG_ERROR("sf::AuthenticationWebBrowserRunner::startWebBrowser::Failed to start web browser on waitpid. err: %s", strerror(errno));
                    throw AuthException(std::string("SFOAuthError ") + strerror(errno));
                }

                if (WIFEXITED(child_status)) {
                    const int es = WEXITSTATUS(child_status);
                    if (es != 0)
                    {
                        CXX_LOG_ERROR("sf::AuthenticationWebBrowserRunner::startWebBrowser::Failed to start web browser. xdg-open returned %d", es);
                        throw AuthException("SFOAuthError " + std::to_string(es));
                    }
                }
            }
#endif
        }
    } // namespace Client
} // namespace Snowflake