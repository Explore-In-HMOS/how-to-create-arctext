import arcTextNapi from 'libentry.so';
import { ArcList, ArcListItem, ArcListAttribute, NodeContent } from '@kit.ArkUI';

@Entry
@Component
struct Index {
  private rootSlot: NodeContent = new NodeContent();

  aboutToAppear(): void {
    // No sizing math needed — native picks sensible defaults from the
    // actual screen size, so the same calls work on 466, 454, 390, ... faces.

    // Top.
    arcTextNapi.drawArcText({ content: this.rootSlot, text: 'Hello ArcText', startAngleDeg: 0, fontSize: 42 });
    // Left.
    arcTextNapi.drawArcText({ content: this.rootSlot, text: 'Small Left Text', startAngleDeg: -90, fontSize: 24 });
    // Right.
    arcTextNapi.drawArcText({ content: this.rootSlot, text: 'Small Right Text', startAngleDeg: 90, fontSize: 24  });
    // Bottom (flipped so it reads upright).
    arcTextNapi.drawArcText({
      content: this.rootSlot,
      text: 'Flipped Text',
      color: 0xFF00E5FF,
      startAngleDeg: 180,
      flipped: true
    });
  }

  build() {
    Stack() {
      // ArkTS Components can be integrated together with the Native C components like this
      ArcList({ initialIndex: 1 }) {
        ArcListItem() {
          Text('Native C++ API Embedded with ArkTS Components')
            .width('80%')
            .margin({ bottom: 8 })
        }

        ArcListItem() {
          this.customCard('Test Card 1')
        }

        ArcListItem() {
          this.customCard('Test Card 2')
        }

        ArcListItem() {
          this.customCard('Test Card 3')
        }
      }
      .width('100%')
      .height('100%')
      .defaultFocus(true)
      .focusable(true)

      // WRAP ContentSlot in a Stack (or Column/Row)
      Stack() {
        ContentSlot(this.rootSlot)
      }
      .width('100%')
      .height('100%')
      // Apply the transparent hit test mode to the WRAPPER
      .hitTestBehavior(HitTestMode.Transparent)

      // Q: Why do we wrap ContentSlot in a Stack, and not apply 'HitTestMode.Transparent' to ContentSlot directly?
      // A: ContentSlot itself doesn't support hitTestBehavior, this way gestures are passed through correctly

    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }

  @Builder
  customCard(customText: string) {
    Text(customText)
      .width('75%')
      .height('20%')
      .fontColor(Color.White)
      .backgroundColor('#ff151515')
      .borderRadius(32)
      .padding({ left: 8, right: 8 })
      .margin({ bottom: 8 })
  }
}
