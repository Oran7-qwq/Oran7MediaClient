import QtQuick 2.15
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../../Basic"
import Client 1.0

Popup{
    id:mainLoginPopup
    Connections
    {
        target: BasicConfig
        function onOpenMainLoginPopup()
        {
            mainLoginPopup.open()
        }
    }

    closePolicy: Popup.NoAutoClose

    background: Rectangle{
        id:mainLoginPopupBackgroundRectangle
        anchors.fill: parent
        color: "#fef2e8"
        radius: 10
        border.color: "#3a3a3d"
        border.width: 2
        //右上角CloseIcon
        Image {
            id: mainLoginPopupCloseImag
            source: "qrc:/image/close.png"
            scale: 1.5
            anchors.right: parent.right
            anchors.rightMargin: 30
            anchors.top: parent.top
            anchors.topMargin: 25
            layer.enabled:false
            layer.effect: ColorOverlay{
                source: mainLoginPopupCloseImag
                color:"white"
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    mainLoginPopupCloseImag.layer.enabled =true
                    cursorShape = Qt.PointingHandCursor
                }
                onExited: {
                    mainLoginPopupCloseImag.layer.enabled = false
                    cursorShape = Qt.ArrowCursor
                }
                onClicked: {
                    mainLoginPopup.close()
                }
            }
        }
        //角形二维码切换Iamge
        Image{
            source: "qrc:/image/tangleqr.png"
            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 16
            scale: 1.5
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    cursorShape = Qt.PointingHandCursor
                }
                onExited: {
                    cursorShape = Qt.ArrowCursor
                }
                onClicked: {
                    BasicConfig.openLoginPopup()
                    mainLoginPopup.close()
                }
            }
        }
        /*logoImage*/
        // Image{
        //     id:logoImage
        //     source: "qrc:/image/logo.png"
        //     anchors.horizontalCenter: parent.horizontalCenter
        //     y:parent.y+80
        // }
        Label{
            id:logoLabel
            text: "Oran柒云音乐"
            anchors.horizontalCenter: parent.horizontalCenter
            y:parent.y+80
            font.family: "华文彩云"
            font.bold: true
            font.pixelSize: 25
            color: "#fc3c55"
        }
        //UpInputRectangle
        Rectangle{
            id:upInputRectangle
            width:280
            height:40
            radius: height/2
            anchors.top: logoLabel.bottom
            anchors.topMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
            color:"#f8c7c7"
            border.color: "#303038"
            border.width: 2
            //"+86"
            Label{
                id:phoneHeaderLabel
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                color: "#2a1a22"
                font.pixelSize: 16
                text:"+86∨"
            }
            //分割线
            Rectangle{
                id:upInputRectangleDive1
                anchors.left:phoneHeaderLabel.right
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 20
                color: "#2a1a22"
            }
            //账号输入TextField
            TextField{
                id:accountNumTextFiled
                text:"16692978550"
                width: 150
                height: parent.height-10
                anchors.verticalCenter: parent.verticalCenter
                anchors.left:upInputRectangleDive1.right
                anchors.leftMargin: 16
                font.family: "微软雅黑"
                font.pixelSize: 14
                maximumLength: 16
                color:"#2a1a22"
                placeholderText : "请输入手机号"
                placeholderTextColor:"#98989b"
                background: Rectangle{
                    anchors.fill: parent
                    color: "transparent"
                }
                onTextChanged: {
                    if(accountNumTextFiled.text ===  "" )
                        clearImage1.visible=false
                    else
                        clearImage1.visible=true
                }
            }
            //清空输入框操作
            Image{
                id:clearImage1
                scale: 0.26
                source: "qrc:/image/clear.png"
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: accountNumTextFiled.right
                anchors.leftMargin:-20
                layer.enabled: false
                visible: false
                layer.effect: ColorOverlay{
                    source:clearImage1
                    color: "white"
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        clearImage1.layer.enabled = true
                    }
                    onExited: {
                        clearImage1.layer.enabled = false
                    }
                    onClicked: {
                        accountNumTextFiled.text = ""
                    }
                }
            }
        }
        //DownInputRectangle
        Rectangle{
            id:downInputRectangle
            width:280
            height:40
            radius: height/2
            anchors.top: upInputRectangle.bottom
            anchors.topMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
            color:"#f8c7c7"
            border.color: "#303038"
            border.width: 2
            TextField{
                id:passWordTextFiled
                text:"lihuijie0"
                width: 200
                height: parent.height-10
                anchors.verticalCenter: parent.verticalCenter
                anchors.left:parent.left
                anchors.leftMargin: 16
                font.family: "微软雅黑"
                font.pixelSize: 14
                maximumLength: 24
                color:"#2a1a22"
                echoMode: TextInput.Password
                placeholderText : "请输入密码"
                placeholderTextColor:"#98989b"
                background: Rectangle{
                    anchors.fill: parent
                    color: "transparent"
                }
                onTextChanged: {
                    if(passWordTextFiled.text ===  "" )
                        clearImage2.visible=false
                    else
                        clearImage2.visible=true
                }
            }
            //清空输入栏Image
            Image{
                id:clearImage2
                scale: 0.26
                source: "qrc:/image/clear.png"
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: passWordTextFiled.right
                anchors.leftMargin:-20
                visible: false
                layer.enabled: false
                layer.effect: ColorOverlay{
                    source:clearImage2
                    color: "white"
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        clearImage2.layer.enabled = true
                    }
                    onExited: {
                        clearImage2.layer.enabled = false
                    }
                    onClicked: {
                        passWordTextFiled.text = ""
                    }
                }
            }
            //isHidePasswordImage
            Image{
                id:hidePasswordImage
                scale: 0.26
                source: "qrc:/image/hideContent.png"
                anchors.left: passWordTextFiled.right
                anchors.leftMargin:-10
                anchors.verticalCenter: parent.verticalCenter
                layer.enabled: false
                layer.effect: ColorOverlay{
                    source:hidePasswordImage
                    color: "white"
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        hidePasswordImage.layer.enabled = true
                    }
                    onExited: {
                        hidePasswordImage.layer.enabled = false
                    }
                    onClicked: {
                        if(passWordTextFiled.echoMode === TextInput.Password)
                        {
                            passWordTextFiled.echoMode = TextInput.Normal
                            hidePasswordImage.source = "qrc:/image/show.png"
                        }
                        else if(passWordTextFiled.echoMode === TextInput.Normal)
                        {
                            passWordTextFiled.echoMode = TextInput.Password
                            hidePasswordImage.source = "qrc:/image/hideContent.png"
                        }
                    }
                }
            }
        }
        // mulFunctionRectangle登录上方的多功能文字按键区域
        Rectangle{
            id:mainLoginMulFunction
            width: 280
            height:18
            anchors.left: downInputRectangle.left
            anchors.leftMargin: 2
            anchors.top: downInputRectangle.bottom
            anchors.topMargin: 8
            color: "transparent"
            //自动登录勾选框
            Rectangle{
                id:autoLoginRectangle
                width: parent.height
                height: width
                radius: 4
                color: "transparent"
                border.color: "#76767c"
                border.width: 1
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                clip: true
                Image {
                    id: sureAutoLoginImage
                    source: "qrc:/image/sure.png"
                    anchors.fill: parent
                    scale: 1.3
                    opacity: 0
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        if(sureAutoLoginImage.opacity === 0)
                            autoLoginRectangle.border.color = "white"
                    }
                    onExited: {
                        if(sureAutoLoginImage.opacity === 0)
                            autoLoginRectangle.border.color = "#76767c"
                    }
                    onClicked: {
                        if(sureAutoLoginImage.opacity === 0)
                        {
                            sureAutoLoginImage.opacity = 1
                            autoLoginRectangle.border.color = "#ff3a3a"
                            autoLoginRectangle.color = "#ff3a3a"
                        }
                        else
                        {
                            sureAutoLoginImage.opacity = 0
                            autoLoginRectangle.border.color = "white"
                            autoLoginRectangle.color = "transparent"
                        }
                    }
                }
            }
            //自动登录TextLabel
            Label{
                id:autoLoginLabel
                text:"自动登录"
                color:"#76767c"
                font.family: "微软雅黑"
                font.pixelSize: 14
                anchors.left: autoLoginRectangle.right
                anchors.leftMargin: 4
                anchors.top: parent.top
                anchors.topMargin: -1

            }
            //验证码登录TextLabel
            Label{
                id:checkCodeLabel
                text:"验证码登录"
                color:"#bababd"
                font.family: "微软雅黑"
                font.pixelSize: 13
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                layer.enabled: false
                layer.effect: ColorOverlay{
                    source: checkCodeLabel
                    color:"white"
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        checkCodeLabel.layer.enabled = true
                    }
                    onExited: {
                        checkCodeLabel.layer.enabled = false
                    }
                    onClicked: {
                        //---
                    }
                }
            }
            //分割线
            Rectangle{
                id:mainLoginMulFunctionDive1
                width: 1
                height: parent.height
                anchors.right:checkCodeLabel.left
                anchors.rightMargin: 4
                color: "#76767c"
            }
            //忘记密码TextLabel
            Label{
                id:forgetPasswordLabel
                text:"忘记密码"
                color:"#bababd"
                font.family: "微软雅黑"
                font.pixelSize: 13
                anchors.right: mainLoginMulFunctionDive1.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                layer.enabled: false
                layer.effect: ColorOverlay{
                    source: forgetPasswordLabel
                    color:"white"
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        forgetPasswordLabel.layer.enabled = true
                    }
                    onExited: {
                        forgetPasswordLabel.layer.enabled = false
                    }
                    onClicked: {
                        //---
                    }
                }
            }
            //登录格式正确提醒TextLabel
            Label{
                id:isValidFormat
                text:""
                color:"red"
                font.family: "微软雅黑"
                font.pixelSize: 14
                anchors.top: autoLoginRectangle.bottom
                anchors.topMargin: 3
                anchors.left: autoLoginRectangle.left
            }
            Connections{
                target: Client
                function onLoginInputFormatValid(isValid)
                {
                    switch(isValid)
                    {
                    case 0:isValidFormat.text="";
                        break;
                    case 1:isValidFormat.text="账号格式错误";
                        break;
                    case 2:isValidFormat.text="密码格式错误";
                        break;
                    case 3:isValidFormat.text="成功登录,加载中......";
                        break;
                    case 4:isValidFormat.text="账号不存在或未注册";
                        break;
                    case 5:isValidFormat.text="密码错误";
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        //大红的登录按钮区域
        Rectangle{
            id:loginRectangle
            width: 280
            height: 40
            radius:height/2
            anchors.horizontalCenter: parent.horizontalCenter
            y:mainLoginMulFunction.y+60
            gradient:Gradient{
                orientation:Gradient.Horizontal
                GradientStop{color: "#ff1168";position: 0.0}
                GradientStop{color: "#fc3d49";position: 1.0}
            }
            BrightnessContrast{
                id:loginRectangleBrightnesss
                source: loginRectangle
                anchors.fill: loginRectangle
                brightness: 0.0
                contrast: 0.0
            }

            ParallelAnimation{
                id:loginRectangleParallelAnimation1
                PropertyAnimation{
                    target: loginRectangle
                    property: "width"
                    from: loginRectangle.width
                    to:240
                    duration: 20
                }
                PropertyAnimation{
                    target: loginRectangle
                    property: "height"
                    from:loginRectangle.height
                    to:32
                    duration: 20
                }
                NumberAnimation{
                    target: loginRectangleBrightnesss
                    property: "brightness"
                    from: loginRectangleBrightnesss.brightness
                    duration: 20
                    to:-0.2
                }
                NumberAnimation{
                    target: loginRectangle
                    property: "y"
                    from: loginRectangle.y
                    to:mainLoginMulFunction.y+65
                    duration: 20
                }
            }
            ParallelAnimation{
                id:loginRectangleParallelAnimation2
                PropertyAnimation{
                    target: loginRectangle
                    property: "width"
                    from: loginRectangle.width
                    to:280
                    duration: 20
                }
                PropertyAnimation{
                    target: loginRectangle
                    property: "height"
                    from:loginRectangle.height
                    to:40
                    duration: 20
                }
                NumberAnimation{
                    target: loginRectangleBrightnesss
                    property: "brightness"
                    from: loginRectangleBrightnesss.brightness
                    duration:20
                    to:0.0
                }
                NumberAnimation{
                    target: loginRectangle
                    property: "y"
                    from: loginRectangle.y
                    to:mainLoginMulFunction.y+60
                    duration: 20
                }
            }
            MouseArea{
                anchors.fill: parent
                onPressed: {
                    loginRectangleParallelAnimation1.start()
                }
                onReleased: {
                    loginRectangleParallelAnimation2.start()
                }
                onClicked: {
                    Client.validateLogin(accountNumTextFiled.text,passWordTextFiled.text);
                }
            }
            //登录标签
            Label{
                id:loginTextLabel
                anchors.centerIn: parent
                font.family: "微软雅黑 Light"
                font.bold: true
                font.pixelSize: 18
                color: "white"
                text: "登录"
            }
        }
        //四个登录方式的Row区域
        Row{
            id:loginWaysRow
            width: 280
            height:36
            anchors.bottom:parent.bottom
            anchors.bottomMargin: 100
            anchors.left:parent.left
            anchors.leftMargin: 140
            spacing: 25
            Repeater{
                id:loginWaysRectangleRepeater
                anchors.fill: parent
                model:["qrc:/image/wechat.png",
                            "qrc:/image/qq.png"]
                delegate:Rectangle{
                    id:wayRectangle
                    height: loginWaysRow.height
                    width: height
                    radius: height/2
                    anchors.verticalCenter: loginWaysRow.verticalCenter
                    border.width: 1
                    border.color: "#8e8e93"
                    color: "#fef2e8"
                    Image {
                        id:wayImage
                        scale:1.0
                        source: modelData
                        anchors.centerIn: parent
                        layer.enabled: false
                        layer.effect:ColorOverlay{
                            source: wayImage
                            color: index === 0 ? "#4d9c3c" :
                                        (index=== 1 ? "#328dc4":
                                        (index === 2 ? "#af172b" :
                                        (index === 3 ? "#af172b" : "")))
                        }
                    }

                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            wayImage.layer.enabled = true
                            wayRectangle.border.color = index === 0 ? "#427e36" :
                                                                      (index=== 1 ? "#2d739f":
                                                                      (index === 2 ? "#af172b" :
                                                                      (index === 3 ? "#af172b" : "")))
                            wayRectangle.color = index === 0 ? "#222d27" :
                                                               (index=== 1 ? "#1e2b3a":
                                                               (index === 2 ? "#531927" :
                                                               (index === 3 ? "#531927" : "")))
                            cursorShape = Qt.PointingHandCursor
                        }
                        onExited: {
                            wayImage.layer.enabled = false
                            wayRectangle.border.color = "#8e8e93"
                            wayRectangle.color = "#fef2e8"
                            cursorShape = Qt.ArrowCursor
                        }
                        onClicked: {
                            //---
                        }
                    }
                }
            }
        }
        //同意协议CheckBox
        Rectangle{
            id:bottomCheckBoxRectangle
            width: 20
            height: width
            radius: width/2
            color: "transparent"
            border.color: "#76767c"
            border.width: 1
            anchors.bottom:parent.bottom
            anchors.bottomMargin: 20
            anchors.left: downInputRectangle.left
            anchors.leftMargin: -20
            clip: true
            // Image {
            //     id: bottomCheckBoxImage
            //     source: "qrc:/image/sure.png"
            //     anchors.fill: parent
            //     scale: 2
            //     opacity: 0
            // }
            BorderImage {
                id: bottomCheckBoxImage
                source: "qrc:/image/sure.png"
                width: bottomCheckBoxRectangle.width; height: bottomCheckBoxRectangle.width
                border.left: bottomCheckBoxRectangle.width/2; border.top: bottomCheckBoxRectangle.width/2
                border.right: bottomCheckBoxRectangle.width/2; border.bottom: bottomCheckBoxRectangle.width/2
                opacity: 0
            }
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    if(bottomCheckBoxImage.opacity === 0)
                        bottomCheckBoxRectangle.border.color = "#616161"
                }
                onExited: {
                    if(bottomCheckBoxImage.opacity === 0)
                        bottomCheckBoxRectangle.border.color = "#76767c"
                }
                onClicked: {
                    if(bottomCheckBoxImage.opacity === 0)
                    {
                        bottomCheckBoxImage.opacity = 1
                        bottomCheckBoxRectangle.border.color = "#ff3a3a"
                        bottomCheckBoxRectangle.color = "#ff3a3a"
                    }
                    else
                    {
                        bottomCheckBoxImage.opacity = 0
                        bottomCheckBoxRectangle.border.color = "#616161"
                        bottomCheckBoxRectangle.color = "transparent"
                    }
                }
            }
        }
        //同意协议Text
        Row{
            id:bottomTextRow
            width: 280
            height:20
            anchors.left: bottomCheckBoxRectangle.right
            anchors.leftMargin: 10
            anchors.verticalCenter: bottomCheckBoxRectangle.verticalCenter
            spacing: 0
            Label{
                id:agreeLabel
                text:"同意"
                anchors.verticalCenter: parent.verticalCenter
                font.family: "微软雅黑"
                font.pixelSize: 14
                color: "#8d8d92"
            }
            Label{
                id:serveLabel
                text: "《服务条款》"
                font.family: "微软雅黑"
                font.pixelSize: 14
                color:"#5c7ab9"
            }
            Label{
                id:privateLabel
                text: "《隐私政策》"
                font.family: "微软雅黑"
                font.pixelSize: 14
                color:"#5c7ab9"
            }
            Label{
                id:childLabel
                text: "《儿童隐私政策》"
                font.family: "微软雅黑"
                font.pixelSize: 14
                color:"#5c7ab9"
            }
        }
    }
}

