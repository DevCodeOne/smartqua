import React, { Component } from 'react'
import { Link } from 'react-router-dom'
import { BottomNavigation, BottomNavigationAction } from '@material-ui/core'
import HomeIcon from '@material-ui/icons/Home'
import SettingsIcon from '@material-ui/icons/Settings'

export class Navigation extends Component {

    render() {
        const toBottom = {
                    width: '100%',
                    position : 'fixed',
                    bottom : 0
                };
        return (
            <div>
                <BottomNavigation showLabels style={toBottom}>
                    <BottomNavigationAction component={Link} to={"/"} label="Home" icon={<HomeIcon />} />
                    <BottomNavigationAction component={Link} to={"/Settings"} label="Settings" icon={<SettingsIcon />} />
                </BottomNavigation>
            </div>
        )
    }
}