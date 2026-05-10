import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import QtQuick.Controls
import "../../Basic"
import Client 1.0

Item{
    id:middleTopMenuItem

    Row{
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        //UserIcon
        Connections{
            target: Client
            function onLoginSuccess(username_)
            {
                console.log("loginSuccess")
                mainLoginPopup.close()
                username.text=username_
                BasicConfig.isLogin=true
            }
        }
        Rectangle{
            id:userIconRectangle
            height: 25
            width: height
            radius: width/2
            color: "#fef2e8"
            Image {
                id: userIconImgae
                anchors.centerIn: parent
                source: "/image/usericon.png"
            }
        }
        //UserName
        Text{
            id:username
            text:"未登录"
            color:"#2a1a22"
            font.pixelSize: 14
            font.family: "微软雅黑 Light"
            anchors.verticalCenter: userIconRectangle.verticalCenter
            width:60
            MouseArea{
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    username.color = "white"
                }
                onExited: {
                    username.color = "#2a1a22"
                }
                onClicked: {
                    if(BasicConfig.isLogin===false)
                        BasicConfig.openLoginPopup()
                    else if(BasicConfig.isLogin===true)
                    {
                        //---
                    }
                }
            }
        }
        //UserVIPCard
        Item{
            id:vipItem
            height: userIconRectangle.height
            width: 58
            anchors.verticalCenter: userIconRectangle.verticalCenter

            visible: false//暂时Discard

            Rectangle{
                id:vipRectangle
                width: parent.width
                height: (parent.height+1)/2
                color:"#a1a1a3"
                radius: height/2
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
            }
            Label{
                text:"VIP开通>"
                font.pixelSize: 8
                font.family: "微软雅黑 Light"
                horizontalAlignment: Text.AlignRight
                color: "#2a1a22"
                anchors.verticalCenter: vipRectangle.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 20
            }
            Rectangle{
                id:recard
                width: vipRectangle.height+5
                height: width
                radius: width/2
                color:"#a1a1a3"
                border.width: 1
                border.color:"#2a1a22"
                anchors.verticalCenter: vipRectangle.verticalCenter
            }
            Rectangle{
                id:innerRecard
                width:recard.width/3
                height: width
                radius: height/2
                color:"white"
                anchors.verticalCenter: recard.verticalCenter
                anchors.horizontalCenter: recard.horizontalCenter
            }
        }
        //User 'v'
        Image{
            id:userAccountv
            source: "/image/account.png"
            anchors.verticalCenter: userIconRectangle.verticalCenter
            layer.enabled: true
            property string userAccountvColorOverlay_Color: "#2a1a22"
            layer.effect: ColorOverlay{
                source: userAccountv
                color: userAccountv.userAccountvColorOverlay_Color
            }
            MouseArea{
                anchors.fill:parent
                hoverEnabled: true
                onEntered: {
                    // userAccountv.layer.enabled = true
                    userAccountv.userAccountvColorOverlay_Color = "#616161"
                    username.color = "#616161"
                }
                onExited: {
                    // userAccountv.layer.enabled = false
                    userAccountv.userAccountvColorOverlay_Color = "#2a1a22"
                    username.color = "#2a1a22"
                }
                onClicked: {
                    if(BasicConfig.isLogin===false)
                        BasicConfig.openLoginPopup()
                    else if(BasicConfig.isLogin===true)
                    {
                        //---
                    }
                }
            }
        }
        //MessageCenter
        Image{
            id:messageCenterImage
            source: "/image/message.png"
            anchors.verticalCenter: userIconRectangle.verticalCenter
            property string messageCenterImageColorOverLay_Color: "#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: messageCenterImage
                color: messageCenterImage.messageCenterImageColorOverLay_Color
            }
            MouseArea{
                anchors.fill:parent
                hoverEnabled: true
                onEntered: {
                    messageCenterImage.messageCenterImageColorOverLay_Color = "#616161"
                }
                onExited: {
                    messageCenterImage.messageCenterImageColorOverLay_Color = "#2a1a22"
                }
                onClicked: {
                    //---
                }
            }
        }
        //Setting
        Image{
            id:settingImage
            source: "/image/set.png"
            anchors.verticalCenter: userIconRectangle.verticalCenter
            property string settingImageColorOverLay_Color:"#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: settingImage
                color: settingImage.settingImageColorOverLay_Color
            }
            MouseArea{
                anchors.fill:parent
                hoverEnabled: true
                onEntered: {
                    settingImage.settingImageColorOverLay_Color = "#616161"
                }
                onExited: {
                    settingImage.settingImageColorOverLay_Color = "#2a1a22"
                }
                onClicked: {
                    if(mainStackView.currentItem)
                    {
                        mainStackView.pageManager.navigateTo(BasicConfig.getUrl(BasicConfig.settingPage),"SettingPage")
                        mainStackView.upDateStack()
                    }
                }
            }
        }
        //SkinImage
        Image{
            id:skinImage
            source: "/image/skin.png"
            anchors.verticalCenter: userIconRectangle.verticalCenter
            property string skinImageColorOverLay_Color: "#2a1a22"
            layer.enabled: true
            layer.effect: ColorOverlay{
                source: skinImage
                color: skinImage.skinImageColorOverLay_Color
            }
            MouseArea{
                anchors.fill:parent
                hoverEnabled: true
                onEntered: {
                    skinImage.skinImageColorOverLay_Color = "#616161"
                }
                onExited: {
                    skinImage.skinImageColorOverLay_Color = "#2a1a22"
                }
                onClicked: {
                    //---
                }
            }
        }
        //" \"
        Rectangle{
            height: userIconRectangle.height
            width: 1
            color: "#2a1a22"
            anchors.verticalCenter: userIconRectangle.verticalCenter
        }
    }
}
