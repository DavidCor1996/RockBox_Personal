import QtQuick
import QtQuick.Window
import QtWebEngine

Window {
    id: root
    width: 310
    height: 170
    /* The offscreen Qt platform keeps this out of the desktop while a visible
       surface ensures Chromium creates and paints its compositor. */
    visible: true

    Component.onCompleted: console.log("RockPod web render:",
                                       Qt.application.arguments)

    function argumentWithPrefix(prefix) {
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            if (Qt.application.arguments[i].indexOf(prefix) === 0)
                return Qt.application.arguments[i].substring(prefix.length)
        }
        return ""
    }

    property string destination: argumentWithPrefix("rockpod-output=")
    property string sourceUrl: argumentWithPrefix("rockpod-url=")
    property string searchQuery: argumentWithPrefix("rockpod-query=")
    property bool searchSubmitted: false

    function capturePage() {
        page.grabToImage(function(result) {
            if (!root.destination || !result.saveToFile(root.destination))
                Qt.exit(3)
            else
                Qt.quit()
        }, Qt.size(root.width, root.height))
    }

    WebEngineView {
        id: page
        anchors.fill: parent
        url: root.sourceUrl || "https://www.google.com/"
        profile: WebEngineProfile {
            storageName: "RockPodBrowser"
            offTheRecord: false
            persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
            /* A current engine identity is required because Google rejects
               the authentic iOS 5 UA before serving search results. The
               viewport and iPod chrome remain the stock-device treatment. */
            httpUserAgent: "Mozilla/5.0 (X11; Linux x86_64) " +
                           "AppleWebKit/537.36 (KHTML, like Gecko) " +
                           "Chrome/124.0 Safari/537.36"
        }

        onLoadingChanged: function(request) {
            if (request.status === WebEngineView.LoadSucceededStatus) {
                if (root.searchQuery && !root.searchSubmitted) {
                    root.searchSubmitted = true
                    page.runJavaScript(
                        "(function(){var q=document.querySelector('textarea[name=q],input[name=q]');" +
                        "if(!q)return false;q.value=" + JSON.stringify(root.searchQuery) +
                        ";q.form.submit();return true;})()")
                } else {
                    settle.start()
                }
            }
            else if (request.status === WebEngineView.LoadFailedStatus) {
                if (root.searchSubmitted)
                    settle.start()
                else
                    Qt.exit(2)
            }
        }
    }

    Timer {
        id: settle
        interval: 2800
        repeat: false
        onTriggered: {
            if (root.searchSubmitted || root.sourceUrl.indexOf("/search?") >= 0) {
                page.runJavaScript("window.scrollTo(0, 150)")
                scrolled.start()
            } else {
                root.capturePage()
            }
        }
    }

    Timer {
        id: scrolled
        interval: 350
        repeat: false
        onTriggered: root.capturePage()
    }

    Timer {
        interval: 20000
        running: true
        repeat: false
        onTriggered: Qt.exit(4)
    }
}
