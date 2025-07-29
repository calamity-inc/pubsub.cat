## Usage

First, you'll need to obtain an endpoint from <https://pubsub.cat/endpoints.json>.

> [!NOTE]
> The TLS endpoint is merely to allow for usage in "secure contexts" and does not provide real security as the private key we use is public.

### WebSocket

- To subscribe to a topic, send a frame with `+` followed by the topic. For example, `+test` to subscribe to topic `test`.
  - When a message is published, a frame is sent, containing the topic and message, joined by a colon. For example, "Hello" sent to topic `test` would be a frame containing `test:Hello`.
- To unsubscribe from a topic, send a frame with `-` followed by the topic.
- To publish a message, send a frame that's a concatenation of `^`, the topic, `:`, and the message. For example, `^test:Hello` to publish "Hello" to topic `test`.

### HTTP

- To publish a message, send a request with path "/pub" and a "topic" query argument. For example, `curl notls.pubsub.cat/pub?topic=test --data "Hello"` to send "Hello" to every subscriber of `test`.

---

A service by Calamity, Inc. • [Status Page](https://status.calamity.inc/)
