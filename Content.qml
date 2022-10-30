import QtQuick

Item {
    Text{
        text: "test"
        SequentialAnimation on x {
            loops: Animation.Infinite
            SmoothedAnimation { to: 50 }
            SmoothedAnimation { to: 0 }
        }
    }
}
