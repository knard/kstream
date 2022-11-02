import QtQuick

Item {
    Text{
        id: t;
        text: "test"
        color: "red"
        SequentialAnimation on x {
            loops: Animation.Infinite
            SmoothedAnimation { to: 50 }
            SmoothedAnimation { to: 0 }
        }
    }
}
