using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Player : MonoBehaviour
{
    private Rigidbody2D _rigidBody = null;
    readonly float _moveSpeed = 10.0f;
    Vector2 _nextMovePoint;

    private void Start() {
        _rigidBody = this.GetComponent<Rigidbody2D>();
    }

    private void Update() {
        if (!_rigidBody)
            return;

        _nextMovePoint = new Vector2(Input.GetAxisRaw("Horizontal"), Input.GetAxisRaw("Vertical"));
    }

    private void FixedUpdate() {
        if (!_rigidBody)
            return;

        _rigidBody.MovePosition(_rigidBody.position + _nextMovePoint * _moveSpeed * Time.fixedDeltaTime);
        //_rigidBody.velocity = _nextMovePoint * _moveSpeed * Time.fixedDeltaTime;
    }
}
