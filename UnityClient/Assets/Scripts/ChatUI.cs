using System.Collections;
using System.Collections.Generic;
using System;
using System.Text;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using UnityEngine.UI;

public class ChatUI : MonoBehaviour
{
    [SerializeField]
    private InputField InputField = null;

    [SerializeField]
    private Transform ChatGroup = null;

    [SerializeField]
    private GameObject ChatNode = null;

    private TcpClient _server = null;
    private Thread _receiveThread = null;
    private string _reservationMessage = string.Empty;

    private const int Port = 27015;
    private const string Address = "127.0.0.1";

    void Start() {
        ConnectServer();
        InputField.gameObject.SetActive(false);
    }

    void Update() {
        if (Input.GetKeyDown(KeyCode.Return)) {
            if (!InputField.gameObject.activeSelf) {
                InputField.gameObject.SetActive(true);
                InputField.ActivateInputField();
                InputField.text = "";
            }
            else {
                if (InputField.text.Length <= 0) {
                    InputField.gameObject.SetActive(false);
                }
                else {
                    SendMessage();
                }
            }
        }

        if (_reservationMessage.Length > 0) {
            NewMessage(_reservationMessage);
            _reservationMessage = string.Empty;
        }
    }

    private void ConnectServer() {
        try {
            _server = new TcpClient(Address, Port);

            _receiveThread = new Thread(new ThreadStart(ReceiveMessage));
            _receiveThread.IsBackground = true;
            _receiveThread.Start();
        }
        catch (Exception e) {
            Debug.Log("connect error " + e);
        }
    }

    private void NewMessage(string message) {
        GameObject newChatNode = Instantiate(ChatNode, ChatGroup);
        Text chatText = newChatNode.GetComponent<Text>();
        chatText.text = message;
    }

    private void ReceiveMessage() {
        if (_server == null) {
            return;
        }

        try {
            byte[] packet = new byte[1024];

            while (true) {
                NetworkStream stream = _server.GetStream();

                string receiveData = string.Empty;

                int bytes = stream.Read(packet, 0, packet.Length);

                if (bytes > 0) {
                    _reservationMessage = Encoding.ASCII.GetString(packet, 0, bytes);
                }
            }
        }
        catch (Exception e) {
            Debug.Log("receive error " + e);
        }
    }

    private void SendMessage() {
        string message = InputField.text;
        NewMessage(message);

        if (_server != null) {
            NetworkStream stream = _server.GetStream();
            if (stream.CanWrite) {
                byte[] strByte = Encoding.UTF8.GetBytes(message);
                stream.Write(strByte, 0, strByte.Length);
            }
        }

        InputField.text = "";
        InputField.ActivateInputField();
    }
}
