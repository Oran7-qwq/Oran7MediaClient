import QtQuick
import QtQuick.Dialogs
 import QtCore

FileDialog{
    id:root
    title:"选择文件"
    nameFilters:["所有文件 (*)","mp3(*.mp3)","flac(*.flac)","mp4(*.mp4)","flv(*.flv)"]
    fileMode:FileDialog.OpenFile
    property string lastOpenFileDir: ""

    property var filesArray: [] //存储选择好的文件
    property bool selectReset: true //下次选择文件是否清空上次的选择缓存

    signal ready()//文件选择完成，发送给外部信号
    currentFolder:{
        if(root.lastOpenFileDir==="")
        {
            var path = StandardPaths.writableLocation(StandardPaths.DesktopLocation)
            if (!path || path === "")
            {
                    path = StandardPaths.writableLocation(StandardPaths.HomeLocation)
            }
        }
        else
        {
            path=root.lastOpenFileDir
        }
        return path
    }
    onAccepted: {
        const currentFolder = this.currentFolder.toString()
        if(root.lastOpenFileDir ===""  || currentFolder !== root.lastOpenFileDir)
        {
            root.lastOpenFileDir=currentFolder
        }
        const files=selectedFiles.map(url => url.toString())
        if(root.selectReset === true)
            filesArray = []
        files.forEach(fileUrl => {
                const localPath = fileUrl.replace("file:///", "")
                root.filesArray.push(localPath)
                console.log("QML==>[Oran7FileDialog:]Current FilePath:", localPath)
        })
        root.ready()
    }
    onRejected: console.log("QML==>[Oran7FileDialog:]Cancel choose files...")
}
